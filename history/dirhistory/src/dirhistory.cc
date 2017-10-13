#include <monsoon/history/dir/dirhistory.h>
#include <monsoon/history/dir/tsdata.h>
#include <monsoon/history/collect_history.h>
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
#include <numeric>
#include <optional>
#include <queue>
#include <iostream>
#include "v2/tsdata.h"
#include <boost/coroutine2/coroutine.hpp>
#include <boost/coroutine2/protected_fixedsize_stack.hpp>

namespace monsoon {
namespace history {


class monsoon_dirhistory_local_ emit_visitor {
 private:
  class monsoon_dirhistory_local_ impl {
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
    time_point next_timepoint() const noexcept;
    std::tuple<time_point, tsdata_vector_type>& get();
    void advance();

   private:
    co_type::pull_type co_;
    std::optional<co_arg_type> val_;
  };

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
  void fixup_visitors_();
  void emit_with_interval_(acceptor_type&, time_range, time_point::duration);
  void emit_without_interval_(acceptor_type&, time_range);

  static auto select_files_(const std::vector<std::shared_ptr<tsdata>>&,
      std::optional<time_point>, std::optional<time_point>)
      -> std::vector<std::shared_ptr<tsdata>>;

  struct tsdata_cmp {
    bool operator()(const std::shared_ptr<tsdata>& x,
        const std::shared_ptr<tsdata>& y) const {
      assert(x != nullptr && y != nullptr);
      return std::get<0>(x->time()) > std::get<0>(y->time());
    }
  };

  std::vector<impl> visitors_;
  std::priority_queue<
      std::shared_ptr<tsdata>,
      std::vector<std::shared_ptr<tsdata>>,
      tsdata_cmp> selected_files_;
  std::function<impl(std::shared_ptr<tsdata>)> transformer_;
};

template<typename Selector>
emit_visitor::emit_visitor(const std::vector<std::shared_ptr<tsdata>>& files,
    Selector& selector, time_range tr, time_point::duration slack) {
  std::optional<time_point> sel_begin, sel_end;
  if (tr.begin().has_value()) sel_begin = tr.begin().value() - slack;
  if (tr.end().has_value()) sel_end = tr.end().value() + slack;

  for (auto f : select_files_(files, sel_begin, sel_end))
    selected_files_.push(f);

  transformer_ = [&selector, &sel_begin, &sel_end](auto&& ptr) {
    std::clog << "Starting emitting file "
        << std::get<0>(ptr->time())
        << " - "
        << std::get<1>(ptr->time())
        << std::endl;
    return impl(std::move(ptr), selector, sel_begin, sel_end);
  };
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

auto dirhistory::time() const -> std::tuple<time_point, time_point> {
  if (files_.empty()) {
    auto rv = time_point::now();
    return std::make_tuple(rv, rv);
  }

#if __cplusplus >= 201703
  return std::transform_reduce(
      std::next(files_.begin()), files_.end(),
      files_.front()->time(),
      [](const auto& x, const auto& y) {
        return std::make_tuple(
            std::min(std::get<0>(x.value()), std::get<0>(y.value())),
            std::max(std::get<1>(x.value()), std::get<1>(y.value())));
      },
      [](const auto& f) {
        return f->time();
      });
#else
  auto iter = files_.begin();
  auto result = (*iter)->time();
  ++iter;
  while (iter != files_.end()) {
    auto ft = (*iter)->time();
    result = std::make_tuple(
        std::min(std::get<0>(result), std::get<0>(ft)),
        std::max(std::get<1>(result), std::get<1>(ft)));
  }
  return result;
#endif
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
    emit_without_interval_(accept_fn, std::move(tr));
}

void emit_visitor::fixup_visitors_() {
  for (;;) {
    // Load next file if visitors is empty.
    if (visitors_.empty() && !selected_files_.empty()) {
      visitors_.push_back(transformer_(selected_files_.top())); // Single element is a heap
      selected_files_.pop();
    }

    // Erase visitor if it has been drained.
    if (!visitors_.front()) {
      std::pop_heap(visitors_.begin(), visitors_.end(), impl_compare());
      visitors_.pop_back();
      continue;
    }

    // Load any files that should be before visitors_.top.
    while (!selected_files_.empty() &&
        std::get<0>(selected_files_.top()->time()) <= visitors_.front().next_timepoint()) {
      visitors_.push_back(transformer_(selected_files_.top()));
      if (visitors_.back())
        std::push_heap(visitors_.begin(), visitors_.end(), impl_compare());
      else
        visitors_.pop_back(); // Empty file.
      selected_files_.pop();
    }
  }
}

void emit_visitor::emit_with_interval_(acceptor_type& accept_fn,
    time_range tr,
    time_point::duration slack) {
  using key_type = std::tuple<group_name, metric_name>;
  using value_type = std::tuple<time_point, metric_value>;
  // Note: we make use of the fact that multimap insert always happens
  // after the last element that compares equal.
  using map_type = std::multimap<key_type, value_type>;

  // Find a begin timestamp, if none was supplied.
  if (!tr.begin().has_value()
      && !(visitors_.empty() && selected_files_.empty())) {
    fixup_visitors_();
    tr.begin(visitors_.front().next_timepoint());
  }

  // Fill sequences.
  map_type map;
  while (tr.end().has_value()
      ? tr.begin().value() <= tr.end().value()
      : !(visitors_.empty() && !selected_files_.empty())) {
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
    while (!(visitors_.empty() && selected_files_.empty())) {
      fixup_visitors_();

      std::pop_heap(visitors_.begin(), visitors_.end(), impl_compare());
      auto& least = visitors_.back();
      if (!least) {
        visitors_.pop_back();
        continue;
      }

      if (least.next_timepoint() > cutoff) {
        std::push_heap(visitors_.begin(), visitors_.end(), impl_compare());
        break;
      }

      if (least.next_timepoint() >= tr.begin().value() - slack) {
        // Add time points to set.
        auto tp_and_entries = least.get();
        const time_point& tp = std::get<0>(tp_and_entries);
        std::for_each(
            std::make_move_iterator(std::get<1>(tp_and_entries).begin()),
            std::make_move_iterator(std::get<1>(tp_and_entries).end()),
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

void emit_visitor::emit_without_interval_(acceptor_type& accept_fn,
    time_range tr) {
  using key_type = std::tuple<group_name, metric_name>;
  using value_type = std::tuple<time_point, metric_value>;
  using key_hash = typename collect_history::metrics_hash;
  using map_type = std::unordered_map<key_type, value_type, key_hash>;

  if (tr.end().has_value() && tr.end().value() < tr.begin().value())
    return;

  fixup_visitors_();
  if (tr.begin().has_value()) { // Emit the before_map.
    map_type before_map;
    while (!visitors_.empty()
        && (!visitors_.front()
            || visitors_.front().next_timepoint() <= tr.begin().value())) {
      std::pop_heap(visitors_.begin(), visitors_.end(), impl_compare());

      auto& least = visitors_.back();
      if (!least) {
        visitors_.pop_back();
        continue;
      }
      auto least_val = least.get();
      const auto& tp = std::get<0>(least_val);
      std::for_each(
          std::make_move_iterator(std::get<1>(least_val).begin()),
          std::make_move_iterator(std::get<1>(least_val).end()),
          [tp, &before_map](auto&& entry) {
            auto key = std::forward_as_tuple(
                std::get<0>(std::move(entry)),
                std::get<1>(std::move(entry)));
            auto value = std::forward_as_tuple(
                tp,
                std::get<2>(std::move(entry)));
            before_map[key] = value; // Replace with newer value.
          });

      least.advance();
      std::push_heap(visitors_.begin(), visitors_.end(), impl_compare());

      fixup_visitors_();
    }

    // Build emit map.
    std::map<time_point, impl::tsdata_vector_type> emit_map;
    std::for_each(
        std::make_move_iterator(before_map.begin()),
        std::make_move_iterator(before_map.end()),
        [&emit_map](auto&& entry) {
          emit_map[std::get<0>(std::move(entry.second))].emplace_back(
              std::get<0>(std::move(entry.first)),
              std::get<1>(std::move(entry.first)),
              std::get<1>(std::move(entry.second)));
        });

    // Emit everything in emit map.
    std::for_each(
        std::make_move_iterator(emit_map.begin()),
        std::make_move_iterator(emit_map.end()),
        [&accept_fn](auto&& entry) {
          accept_fn.accept(std::move(entry.first), std::move(entry.second));
        });
  }

  // Predicate comparing the (group, metric) combination
  // in a (group, metric, value) tuple.
  static const auto CMP =
      [](
          const std::tuple<group_name, metric_name, metric_value>& x,
          const std::tuple<group_name, metric_name, metric_value>& y) {
        return std::tie(std::get<0>(x), std::get<1>(x))
            < std::tie(std::get<0>(y), std::get<1>(y));
      };
  assert(!visitors_.empty() || selected_files_.empty());
  time_point emit_ts;
  impl::tsdata_vector_type emit_data;
  while (tr.end().has_value()
      ? (!visitors_.front()
          || visitors_.front().next_timepoint() < tr.end().value())
      : !visitors_.empty()) {
    auto& least = visitors_.back();
    assert(least);

    auto least_val = least.get();
    if (!emit_data.empty() && std::get<0>(least_val) != emit_ts) {
      // Remove duplicate (group, metric) tuples.
      std::sort(emit_data.begin(), emit_data.end(), CMP);
      emit_data.erase(
          std::unique(emit_data.begin(), emit_data.end(), CMP),
          emit_data.end());

      // Emit to acceptor.
      accept_fn.accept(std::move(emit_ts), std::move(emit_data));

      // Prepar for next emit.
      emit_ts = std::get<0>(least_val);
      emit_data.clear();
    }
    emit_data.insert(emit_data.cend(),
        std::make_move_iterator(std::get<1>(least_val).begin()),
        std::make_move_iterator(std::get<1>(least_val).end()));

    least.advance();
    std::push_heap(visitors_.begin(), visitors_.end(), impl_compare());

    fixup_visitors_();
  }
  if (!emit_data.empty()) {
    // Remove duplicate (group, metric) tuples.
    std::sort(emit_data.begin(), emit_data.end(), CMP);
    emit_data.erase(
        std::unique(emit_data.begin(), emit_data.end(), CMP),
        emit_data.end());

    // Emit to acceptor.
    accept_fn.accept(std::move(emit_ts), std::move(emit_data));
    emit_data.clear();
  }

  assert(!visitors_.empty() || selected_files_.empty());
  if (tr.end().has_value()) {
    map_type after_map;
    while (!visitors_.empty()
        && (!visitors_.front()
            || visitors_.front().next_timepoint() <= tr.begin().value())) {
      std::pop_heap(visitors_.begin(), visitors_.end(), impl_compare());

      auto& least = visitors_.back();
      assert(least);

      auto least_val = least.get();
      const auto& tp = std::get<0>(least_val);
      std::for_each(
          std::make_move_iterator(std::get<1>(least_val).begin()),
          std::make_move_iterator(std::get<1>(least_val).end()),
          [tp, &after_map](auto&& entry) {
            auto key = std::forward_as_tuple(
                std::get<0>(std::move(entry)),
                std::get<1>(std::move(entry)));
            auto value = std::forward_as_tuple(
                tp,
                std::get<2>(std::move(entry)));
            after_map.emplace(key, value); // Only insert if not present.
          });

      least.advance();
      std::push_heap(visitors_.begin(), visitors_.end(), impl_compare());

      fixup_visitors_();
    }

    // Build emit map.
    std::map<time_point, impl::tsdata_vector_type> emit_map;
    std::for_each(
        std::make_move_iterator(after_map.begin()),
        std::make_move_iterator(after_map.end()),
        [&emit_map](auto&& entry) {
          emit_map[std::get<0>(std::move(entry.second))].emplace_back(
              std::get<0>(std::move(entry.first)),
              std::get<1>(std::move(entry.first)),
              std::get<1>(std::move(entry.second)));
        });

    // Emit everything in emit map.
    std::for_each(
        std::make_move_iterator(emit_map.begin()),
        std::make_move_iterator(emit_map.end()),
        [&accept_fn](auto&& entry) {
          accept_fn.accept(std::move(entry.first), std::move(entry.second));
        });
  }
}


bool emit_visitor::impl_compare::operator()(const impl& x, const impl& y)
    const noexcept {
  // Compare empty optionals as greater, so they get erased from the
  // heap the earliest.
  if (!x) return bool(y);
  if (!y) return false;
  return x.next_timepoint() > y.next_timepoint();
}

void emit_visitor::impl::yield_acceptor::accept(time_point tp, vector_type v) {
  impl_(std::make_tuple(std::move(tp), std::move(v)));
}

template<typename Selector>
emit_visitor::impl::impl(std::shared_ptr<tsdata> tsdata_ptr,
    Selector& selector,
    std::optional<time_point> sel_begin,
    std::optional<time_point> sel_end)
: co_(boost::coroutines2::protected_fixedsize_stack(),
      [tsdata_ptr, &selector, sel_begin, sel_end](co_type::push_type& yield) {
        auto ya = yield_acceptor(yield);
        tsdata_ptr->emit(ya, sel_begin, sel_end, selector);
      })
{
  if (co_) val_ = co_.get();
}

auto emit_visitor::impl::next_timepoint() const noexcept -> time_point {
  return std::get<0>(val_.value());
}

auto emit_visitor::impl::get()
-> std::tuple<time_point, tsdata_vector_type>& {
  return val_.value();
}

void emit_visitor::impl::advance() {
  co_();

  if (co_)
    val_ = co_.get();
  else
    val_.reset();
}


}} /* namespace monsoon::history */
