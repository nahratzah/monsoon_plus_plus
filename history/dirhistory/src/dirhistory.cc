#include <monsoon/history/dir/dirhistory.h>
#include <monsoon/history/dir/tsdata.h>
#include <monsoon/interpolate.h>
#include <stdexcept>
#include <utility>
#include <cassert>
#include <cstddef>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <functional>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <memory>
#include <optional>
#include "v2/tsdata.h"
#include <boost/coroutine2/coroutine.hpp>
#include <boost/coroutine2/protected_fixedsize_stack.hpp>

namespace monsoon {
namespace history {


class monsoon_dirhistory_local_ emit_visitor {
 private:
  class impl;
  struct impl_compare {
    monsoon_dirhistory_local_ bool operator()(const impl&, const impl&)
        const noexcept;
  };

 public:
  using acceptor_type = acceptor<group_name, metric_name, metric_value>;
  using acceptor_vector = acceptor_type::vector_type;

  template<typename Selector>
  emit_visitor(const std::vector<std::shared_ptr<tsdata>>&, Selector&,
      time_range, time_point::duration);
  ~emit_visitor() noexcept;

  void operator()(acceptor_type&, time_range, time_point::duration);

 private:
  void emit_with_interval_(acceptor_type&, time_range, time_point::duration);
  void emit_without_interval_(acceptor_type&, time_range, time_point::duration);

  static auto select_files_(const std::vector<std::shared_ptr<tsdata>>&,
      std::optional<time_point>, std::optional<time_point>)
      -> std::vector<std::shared_ptr<tsdata>>;

  std::vector<impl> visitors_;
};

class monsoon_dirhistory_local_ emit_visitor::impl {
 public:
  using tsdata_vector_type =
      tsdata::emit_acceptor<group_name, metric_name, metric_value>::vector_type;

 private:
  using co_arg_type = std::tuple<time_point, tsdata_vector_type>;
  using co_type = boost::coroutines2::coroutine<co_arg_type>;

  class yield_acceptor
  : public tsdata::emit_acceptor<group_name, metric_name, metric_value>
  {
   public:
    explicit yield_acceptor(co_type::push_type& impl) noexcept : impl_(impl) {}
    ~yield_acceptor() noexcept {}
    void accept(time_point, vector_type) override;

   private:
    co_type::push_type& impl_;
  };

 public:
  template<typename Selector>
  impl(std::shared_ptr<tsdata>, Selector&, std::optional<time_point>, std::optional<time_point>);

  explicit operator bool() const noexcept { return bool(co_); }
  std::optional<time_point> next_timepoint() const noexcept;
  std::tuple<time_point, tsdata_vector_type> get() const;
  void advance();

 private:
  mutable co_type::pull_type co_;
};

template<typename Selector>
emit_visitor::emit_visitor(const std::vector<std::shared_ptr<tsdata>>& files,
    Selector& selector, time_range tr, time_point::duration slack) {
  std::optional<time_point> sel_begin, sel_end;
  if (tr.begin().has_value()) sel_begin = tr.begin().value() - slack;
  if (tr.end().has_value()) sel_end = tr.end().value() + slack;

  auto file_sel = select_files_(files, sel_begin, sel_end);
  std::transform(
      std::make_move_iterator(file_sel.begin()),
      std::make_move_iterator(file_sel.end()),
      std::back_inserter(visitors_),
      [&selector, &sel_begin, &sel_end](auto&& ptr) {
        return impl(std::move(ptr), selector, sel_begin, sel_end);
      });
  std::make_heap(visitors_.begin(), visitors_.end(), impl_compare());
}


dirhistory::dirhistory(filesystem::path dir, bool open_for_write)
: dir_(std::move(dir)),
  writable_(open_for_write)
{
  using filesystem::perms;

  if (!filesystem::is_directory(dir_))
    throw std::invalid_argument("dirhistory requires a directory path");
  if (!dir_.is_absolute())
    throw std::invalid_argument("dirhistory requires an absolute path");
  if (open_for_write &&
      !(filesystem::status(dir_).permissions() & perms::owner_write))
    throw std::invalid_argument("dirhistory path is not writable");

  // Scan directory for files to manage.
  std::for_each(
      filesystem::directory_iterator(dir_), filesystem::directory_iterator(),
      [this, open_for_write](const filesystem::path& fname) {
        const auto fstat = filesystem::status(fname);
        if (filesystem::is_regular_file(fstat)) {
          io::fd::open_mode mode = io::fd::READ_WRITE;
          if (!open_for_write ||
              !(fstat.permissions() & perms::owner_write))
            mode = io::fd::READ_ONLY;

          auto fd = io::fd(fname.native(), mode);
          if (tsdata::is_tsdata(fd))
            files_.push_back(tsdata::open(std::move(fd)));
        }
      });

  if (open_for_write) { // Find a write candidate.
    auto fiter = std::find_if(files_.begin(), files_.end(),
        [](const auto& tsdata_ptr) { return tsdata_ptr->is_writable(); });
    if (fiter != files_.end()) {
      write_file_ = *fiter;

      while (++fiter != files_.end()) {
        if ((*fiter)->is_writable()
            && std::get<0>((*fiter)->time()) > std::get<0>(write_file_->time()))
          write_file_ = *fiter;
      }
    }
  }
}

dirhistory::~dirhistory() noexcept {}

void dirhistory::push_back(const time_series& ts) {
  maybe_start_new_file_(ts.get_time());
  write_file_->push_back(ts);
}

auto dirhistory::simple_groups(const time_range& tr) const
-> std::unordered_set<simple_group> {
  std::unordered_set<simple_group> result;

  for (const auto& file : files_) {
    time_point fbegin, fend;
    std::tie(fbegin, fend) = file->time();
    if (fbegin <= tr.end() && fend >= tr.begin()) {
      auto fsubset = file->simple_groups();
#if __cplusplus >= 201703
      if (result.get_allocator() == fsubset.get_allocator())
        result.merge(std::move(fsubset));
      else
#else
        result.insert(
            std::make_move_iterator(fsubset.begin()),
            std::make_move_iterator(fsubset.end()));
#endif
    }
  }

  return result;
}

auto dirhistory::group_names(const time_range& tr) const
-> std::unordered_set<group_name> {
  std::unordered_set<group_name> result;

  for (const auto& file : files_) {
    time_point fbegin, fend;
    std::tie(fbegin, fend) = file->time();
    if (fbegin <= tr.end() && fend >= tr.begin()) {
      auto fsubset = file->group_names();
#if __cplusplus >= 201703
      if (result.get_allocator() == fsubset.get_allocator())
        result.merge(std::move(fsubset));
      else
#else
        result.insert(
            std::make_move_iterator(fsubset.begin()),
            std::make_move_iterator(fsubset.end()));
#endif
    }
  }

  return result;
}

auto dirhistory::untagged_metrics(const monsoon::time_range& tr) const
-> std::unordered_set<std::tuple<simple_group, metric_name>, metrics_hash> {
  std::unordered_set<std::tuple<simple_group, metric_name>, metrics_hash> result;

  for (const auto& file : files_) {
    time_point fbegin, fend;
    std::tie(fbegin, fend) = file->time();
    if (fbegin <= tr.end() && fend >= tr.begin()) {
      auto fsubset = file->untagged_metrics();
#if __cplusplus >= 201703
      if (result.get_allocator() == fsubset.get_allocator())
        result.merge(std::move(fsubset));
      else
#else
        result.insert(
            std::make_move_iterator(fsubset.begin()),
            std::make_move_iterator(fsubset.end()));
#endif
    }
  }

  return result;
}

auto dirhistory::tagged_metrics(const monsoon::time_range& tr) const
-> std::unordered_set<std::tuple<group_name, metric_name>, metrics_hash> {
  std::unordered_set<std::tuple<group_name, metric_name>, metrics_hash> result;

  for (const auto& file : files_) {
    time_point fbegin, fend;
    std::tie(fbegin, fend) = file->time();
    if (fbegin <= tr.end() && fend >= tr.begin()) {
      auto fsubset = file->tagged_metrics();
#if __cplusplus >= 201703
      if (result.get_allocator() == fsubset.get_allocator())
        result.merge(std::move(fsubset));
      else
#else
        result.insert(
            std::make_move_iterator(fsubset.begin()),
            std::make_move_iterator(fsubset.end()));
#endif
    }
  }

  return result;
}

void dirhistory::emit(
    acceptor<group_name, metric_name, metric_value>& accept_fn,
    time_range tr,
    std::unordered_set<std::tuple<group_name, metric_name>, metrics_hash> selector_arg,
    time_point::duration slack) const {
  std::unordered_multimap<group_name, metric_name> selector;
  std::copy(
      std::make_move_iterator(selector_arg.begin()),
      std::make_move_iterator(selector_arg.end()),
      std::inserter(selector, selector.end()));

  auto visitor = emit_visitor(files_, selector, tr, slack);
  visitor(accept_fn, tr, slack);
}

void dirhistory::emit(
    acceptor<group_name, metric_name, metric_value>& accept_fn,
    time_range tr,
    std::unordered_set<std::tuple<simple_group, metric_name>, metrics_hash> selector_arg,
    time_point::duration slack) const {
  std::unordered_multimap<simple_group, metric_name> selector;
  std::copy(
      std::make_move_iterator(selector_arg.begin()),
      std::make_move_iterator(selector_arg.end()),
      std::inserter(selector, selector.end()));

  auto visitor = emit_visitor(files_, selector, tr, slack);
  visitor(accept_fn, tr, slack);
}

void dirhistory::maybe_start_new_file_(time_point tp) {
  using std::to_string;

  if (!writable_) throw std::runtime_error("history is not writable");

  if (write_file_ == nullptr) {
    auto fname = dir_ / decide_fname_(tp);
    io::fd new_file = io::fd::create(fname.native());
    try {
      new_file = io::fd::create(fname.native());
    } catch (...) {
      for (int i = 0; i < 100; ++i) {
        try {
          new_file = io::fd::create(fname.native() + "-" + to_string(i));
        } catch (...) {
          continue;
        }
        break;
      }
    }

    if (!new_file.is_open())
      throw std::runtime_error("unable to create file");
    try {
      auto new_file_ptr = v2::tsdata_v2::new_list_file(std::move(new_file), tp); // write mime header
      files_.push_back(new_file_ptr);
      write_file_ = new_file_ptr; // Fill in write_file_ pointer
    } catch (...) {
      new_file.unlink();
      throw;
    }
  }
}

auto dirhistory::decide_fname_(time_point tp) -> filesystem::path {
  return (std::ostringstream()
      << std::setfill('0')
      << std::right
      << "monsoon-"
      << std::setw(4) << tp.year()
      << std::setw(2) << tp.month()
      << std::setw(2) << tp.day_of_month()
      << "-"
      << std::setw(2) << tp.hour()
      << std::setw(2) << tp.minute()
      << ".tsd")
      .str();
}


auto emit_visitor::select_files_(
    const std::vector<std::shared_ptr<tsdata>>& files,
    std::optional<time_point> sel_begin,
    std::optional<time_point> sel_end)
-> std::vector<std::shared_ptr<tsdata>> {
  std::vector<std::shared_ptr<tsdata>> file_sel;
  std::copy_if(files.begin(), files.end(), std::back_inserter(file_sel),
      [&sel_begin, &sel_end](const std::shared_ptr<tsdata>& file_ptr) {
        auto[fbegin, fend] = file_ptr->time();
        return (!sel_end.has_value() || fbegin <= sel_end.value())
            && (!sel_begin.has_value() || fend >= sel_begin.value());
      });
  return file_sel;
}

emit_visitor::~emit_visitor() noexcept {}

void emit_visitor::operator()(acceptor_type& accept_fn, time_range tr,
    time_point::duration slack) {
  if (tr.interval().has_value())
    emit_with_interval_(accept_fn, std::move(tr), std::move(slack));
  else
    emit_without_interval_(accept_fn, std::move(tr), slack);
}

void emit_visitor::emit_with_interval_(acceptor_type& accept_fn, time_range tr,
    time_point::duration slack) {
  using key_type = std::tuple<group_name, metric_name>;
  using value_type = std::tuple<time_point, metric_value>;
  // Note: we make use of the fact that multimap insert always happens
  // after the last element that compares equal.
  using map_type = std::multimap<key_type, value_type>;

  // Find a begin timestamp, if none was supplied.
  while (!tr.begin().has_value() && !visitors_.empty()) {
    std::pop_heap(visitors_.begin(), visitors_.end(), impl_compare());
    auto& least = visitors_.back();
    if (!least) {
      visitors_.pop_back();
      continue;
    }

    assert(least.next_timepoint().has_value());
    assert(!tr.begin().has_value());
    tr.begin(least.next_timepoint().value());
    std::push_heap(visitors_.begin(), visitors_.end(), impl_compare());
  }

  // Fill sequences.
  map_type map;
  while (!visitors_.empty()
      && !map.empty()
      && (!tr.end().has_value() || tr.begin().value() <= tr.end().value())) {
    // Erase all values before the low cutoff point.
    for (auto iter = map.begin(); iter != map.end(); ) {
      const auto sibling_iter = std::next(iter);
      const time_point& iter_timestamp = std::get<0>(iter->second);

      if (iter_timestamp < tr.begin().value() - slack) {
        // Erase if iter is too old.
        iter = map.erase(iter);
      } else if (sibling_iter != map.end()
          && iter->first == sibling_iter->first
          && std::get<0>(sibling_iter->second) <= tr.begin().value()) {
        // Erase iter if it is followed by an element that:
        // - has the same key
        // - has a later timestamp, that is before/at the selected timestamp
        iter = map.erase(iter);
      } else {
        // Keep element and advance to next different key.
        if (sibling_iter == map.end() || sibling_iter->first != iter->first)
          iter = std::move(sibling_iter);
        else
          iter = map.upper_bound(iter->first);
      }
    }

    // Load all values up to and including the cutoff timepoint.
    const auto cutoff = tr.begin().value() + slack;
    while (!visitors_.empty()) {
      std::pop_heap(visitors_.begin(), visitors_.end(), impl_compare());
      auto& least = visitors_.back();
      if (!least) {
        visitors_.pop_back();
        continue;
      }

      if (least.next_timepoint().value() > cutoff) {
        std::push_heap(visitors_.begin(), visitors_.end(), impl_compare());
        break;
      }

      if (least.next_timepoint().value() >= tr.begin().value() - slack) {
        // Add time points to set.
        time_point tp;
        impl::tsdata_vector_type entries;
        std::tie(tp, entries) = least.get();
        std::for_each(
            std::make_move_iterator(entries.begin()),
            std::make_move_iterator(entries.end()),
            [&map, tp](auto&& v) {
              // Emplace after any items with same key.
              // Since timestamps are only incremented,
              // this keeps the timestamps in the values ordered.
              const auto ins_pos = map.emplace(
                  std::forward_as_tuple(
                      std::get<0>(std::move(v)),
                      std::get<1>(std::move(v))),
                  std::forward_as_tuple(
                      tp,
                      std::get<2>(std::move(v))));
              // Assertion: requirement of std::multimap emplace.
              assert(std::next(ins_pos) == map.end()
                  || std::next(ins_pos)->first != ins_pos->first);

              if (ins_pos != map.begin()
                  && ins_pos->first == std::prev(ins_pos)->first)
                map.erase(std::prev(ins_pos));
            });
      }

      least.advance();
      std::push_heap(visitors_.begin(), visitors_.end(), impl_compare());
    }

    // Compile values for the to-be-emitted timestamp.
    acceptor_vector values;
    for (auto iter = map.begin();
        iter != map.end();
        iter = map.upper_bound(iter->first)) {
      if (std::get<0>(iter->second) == tr.begin().value()) {
        values.emplace_back(
            std::get<0>(iter->first),
            std::get<1>(iter->first),
            std::get<1>(iter->second));
        continue;
      }

      const auto successor = std::next(iter);
      if (successor->first != iter->first)
        continue; // Not enough data to perform interpolation.

      std::optional<metric_value> opt_mv =
          interpolate(tr.begin().value(), iter->second, successor->second);
      if (opt_mv.has_value()) {
        values.emplace_back(
            std::get<0>(iter->first),
            std::get<1>(iter->first),
            std::move(opt_mv).value());
      }
    }

    // Call out with computed values.
    accept_fn.accept(tr.begin().value(), std::move(values));

    // Increment timestamp.
    if (tr.end().has_value()
        && tr.begin().value() > tr.end().value() - tr.interval().value())
      tr.begin(tr.end().value());
    else
      tr.begin(tr.begin().value() + tr.interval().value());
  }
}


bool emit_visitor::impl_compare::operator()(const impl& x, const impl& y)
    const noexcept {
  // Compare empty optionals as greater, so they get erased from the
  // heap the earliest.
  if (!x.next_timepoint().has_value())
    return y.next_timepoint().has_value();
  if (!y.next_timepoint().has_value())
    return false;
  return x.next_timepoint().value() > y.next_timepoint().value();
}

void emit_visitor::impl::yield_acceptor::accept(time_point tp, vector_type v) {
  impl_(std::make_tuple(std::move(tp), std::move(v)));
}

template<typename Selector>
emit_visitor::impl::impl(std::shared_ptr<tsdata> tsdata_ptr,
    Selector& selector,
    std::optional<time_point> sel_begin,
    std::optional<time_point> sel_end)
: co_(boost::coroutines2::protected_fixedsize_stack(8192),
      [tsdata_ptr, &selector, sel_begin, sel_end](co_type::push_type& yield) {
        auto ya = yield_acceptor(yield);
        tsdata_ptr->emit(ya, sel_begin, sel_end, selector);
      })
{}

auto emit_visitor::impl::next_timepoint() const noexcept -> std::optional<time_point> {
  if (!co_) return {};
  return std::get<0>(co_.get());
}

auto emit_visitor::impl::get() const
-> std::tuple<time_point, tsdata_vector_type> {
  return co_.get();
}

void emit_visitor::impl::advance() {
  co_();
}


}} /* namespace monsoon::history */
