#include <monsoon/history/dir/dirhistory.h>
#include <monsoon/history/dir/tsdata.h>
#include <monsoon/history/collect_history.h>
#include <monsoon/interpolate.h>
#include <monsoon/objpipe/callback.h>
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
#include <iterator>
#include "v2/tsdata.h"
#include "emit_visitor.h"

namespace monsoon {
namespace history {


monsoon_dirhistory_local_
auto dirhistory_emit_reducer(time_point,
    const emit_visitor<tsdata::emit_map>::reduce_queue&)
    -> std::tuple<time_point, tsdata::emit_map>;
monsoon_dirhistory_local_
void dirhistory_emit_merger(std::tuple<time_point, tsdata::emit_map>&,
    std::tuple<time_point, tsdata::emit_map&&>&&);
monsoon_dirhistory_local_
void dirhistory_emit_prune_before_(emit_visitor<tsdata::emit_map>::pruning_vector&);
monsoon_dirhistory_local_
void dirhistory_emit_prune_after_(emit_visitor<tsdata::emit_map>::pruning_vector&);


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
    if ((!tr.end().has_value() || fbegin <= tr.end().value())
        && (!tr.begin().has_value() || fend >= tr.begin().value())) {
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

auto dirhistory::emit(
    time_range tr,
    std::function<bool(const group_name&)> group_filter,
    std::function<bool(const group_name&, const metric_name&)> metric_filter,
    time_point::duration slack) const -> objpipe::reader<emit_type> {
  using namespace std::placeholders;

  auto visitor = emit_visitor<tsdata::emit_map>(
      files_, tr, std::move(slack),
      [group_filter, metric_filter](const tsdata& tsd, const auto& cb,
          const auto& tr_begin, const auto& tr_end) {
        tsd.emit(cb, tr_begin, tr_end, group_filter, metric_filter);
      },
      &dirhistory_emit_merger,
      &dirhistory_emit_reducer,
      &dirhistory_emit_prune_before_,
      &dirhistory_emit_prune_after_);

  return objpipe::new_callback<emit_type>(
      std::bind(
          [](emit_visitor<tsdata::emit_map>& visitor, auto& cb) {
            visitor(
                [&cb](time_point tp, tsdata::emit_map&& map) {
                  cb(emit_type(std::in_place_index<1>, std::move(tp), std::move(map)));
                  std::invoke(cb, emit_type(std::in_place_index<1>, std::move(tp), std::move(map)));
                });
          },
          std::move(visitor),
          _1));
}

auto dirhistory::emit(
    time_range tr,
    std::function<bool(const simple_group&)> group_filter,
    std::function<bool(const simple_group&, const metric_name&)> metric_filter,
    time_point::duration slack) const -> objpipe::reader<emit_type> {
  using namespace std::placeholders;

  auto visitor = emit_visitor<tsdata::emit_map>(
      files_, tr, std::move(slack),
      [group_filter, metric_filter](const tsdata& tsd, const auto& cb,
          const auto& tr_begin, const auto& tr_end) {
        tsd.emit(cb, tr_begin, tr_end, group_filter, metric_filter);
      },
      &dirhistory_emit_merger,
      &dirhistory_emit_reducer,
      &dirhistory_emit_prune_before_,
      &dirhistory_emit_prune_after_);

  return objpipe::new_callback<emit_type>(
      std::bind(
          [](emit_visitor<tsdata::emit_map>& visitor, auto& cb) {
            visitor(
                [&cb](time_point tp, tsdata::emit_map&& map) {
                  std::invoke(cb, std::forward_as_tuple(tp, std::move(map)));
                });
          },
          std::move(visitor),
          _1));
}

auto dirhistory::emit_time(
    time_range tr,
    time_point::duration slack) const -> objpipe::reader<time_point> {
  return objpipe::new_callback<time_point>(
      emit_visitor<>(
          files_, tr, std::move(slack),
          [](const tsdata& tsd, const auto& cb, const auto& tr_begin, const auto& tr_end) {
            tsd.emit_time(cb, tr_begin, tr_end);
          },
          [](auto&, auto&&) {},
          [](time_point at, const auto&) { return at; },
          [](emit_visitor<>::pruning_vector& v) {
            if (!v.empty()) v.erase(v.begin(), std::prev(v.end()));
          },
          [](emit_visitor<>::pruning_vector& v) {
            if (!v.empty()) v.erase(std::next(v.begin()), v.end());
          }));
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


auto dirhistory_emit_reducer(time_point at,
    const emit_visitor<tsdata::emit_map>::reduce_queue& q)
-> std::tuple<time_point, tsdata::emit_map> {
  // Hash function for intermediate map.
  struct intermediate_hash {
    std::size_t operator()(const tsdata::emit_map::key_type*const& key) const {
      tsdata::emit_map::hasher h;
      assert(key != nullptr);
      return h(*key);
    }
  };

  // Key equality function for intermediate map.
  struct intermediate_eq {
    bool operator()(const tsdata::emit_map::key_type*const& x,
        const tsdata::emit_map::key_type*const& y) const {
      tsdata::emit_map::key_equal e;
      assert(x != nullptr && y != nullptr);
      return e(*x, *y);
    }
  };

  std::tuple<time_point, tsdata::emit_map> result_tpl;
  std::get<0>(result_tpl) = at;
  tsdata::emit_map& result = std::get<1>(result_tpl);
  auto intermediate = std::unordered_map<
      const typename tsdata::emit_map::key_type*,
      std::tuple<time_point, const metric_value*>,
      intermediate_hash,
      intermediate_eq>();

  auto q_iter = q.begin(), q_end = q.end();

  while (q_iter != q_end && std::get<0>(*q_iter) < at) {
    for (const auto& entry : std::get<1>(*q_iter))
      intermediate.insert_or_assign(
          &entry.first,
          std::make_tuple(std::get<0>(*q_iter), &entry.second));
  }

  if (q_iter != q_end && std::get<0>(*q_iter) == at)
    result = std::get<1>(*q_iter);

  while (q_iter != q_end) {
    assert(std::get<0>(*q_iter) > at);
    for (const auto& entry : std::get<1>(*q_iter)) {
      // Skip definite entries.
      if (result.find(entry.first) != result.end()) continue;

      // Interpolate value, if the before range contains it.
      auto found = intermediate.find(&entry.first);
      if (found != intermediate.end()) {
        std::optional<metric_value> mv = interpolate(
            at,
            std::tie(
                std::get<0>(found->second),
                *std::get<1>(found->second)),
            std::tie(
                std::get<0>(*q_iter),
                entry.second));
        if (mv.has_value())
          result.emplace(entry.first, std::move(mv).value());
      }
    }
  }

  return result_tpl;
}

void dirhistory_emit_merger(std::tuple<time_point, tsdata::emit_map>& dst_tpl,
    std::tuple<time_point, tsdata::emit_map&&>&& src_tpl) {
  tsdata::emit_map& dst = std::get<1>(dst_tpl);
  tsdata::emit_map&& src = std::get<1>(std::move(src_tpl));

#if __cplusplus >= 201703
  if (dst.get_allocator() == src.get_allocator())
    dst.merge(std::move(src));
  else
#endif
    std::copy(
        std::make_move_iterator(src.begin()),
        std::make_move_iterator(src.end()),
        std::inserter(dst, dst.begin()));
}

void dirhistory_emit_prune_before_(emit_visitor<tsdata::emit_map>::pruning_vector& v) {
  using key_set = std::unordered_set<
      tsdata::emit_map::key_type,
      tsdata::emit_map::hasher,
      tsdata::emit_map::key_equal>;
  using namespace std::placeholders;

  key_set keys;
  for (auto iter = v.rbegin(); iter != v.rend(); ++iter) {
    std::for_each(keys.cbegin(), keys.cend(),
        [&iter](const key_set::value_type& k) {
          std::get<1>(*iter).erase(k);
        });

    std::transform(std::get<1>(*iter).cbegin(), std::get<1>(*iter).cend(),
        std::inserter(keys, keys.begin()),
        std::bind(&tsdata::emit_map::value_type::first, _1));
  }

  v.erase(
      std::remove_if(
          v.begin(), v.end(),
          [](const auto& v) { return std::get<1>(v).empty(); }),
      v.end());
}

void dirhistory_emit_prune_after_(emit_visitor<tsdata::emit_map>::pruning_vector& v) {
  using key_set = std::unordered_set<
      tsdata::emit_map::key_type,
      tsdata::emit_map::hasher,
      tsdata::emit_map::key_equal>;
  using namespace std::placeholders;

  key_set keys;
  for (auto iter = v.begin(); iter != v.end(); ++iter) {
    std::for_each(keys.cbegin(), keys.cend(),
        [&iter](const key_set::value_type& k) {
          std::get<1>(*iter).erase(k);
        });

    std::transform(std::get<1>(*iter).cbegin(), std::get<1>(*iter).cend(),
        std::inserter(keys, keys.begin()),
        std::bind(&tsdata::emit_map::value_type::first, _1));
  }

  v.erase(
      std::remove_if(
          v.begin(), v.end(),
          [](const auto& v) { return std::get<1>(v).empty(); }),
      v.end());
}


}} /* namespace monsoon::history */
