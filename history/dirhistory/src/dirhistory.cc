#include <monsoon/history/dir/dirhistory.h>
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <numeric>
#include <optional>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <monsoon/history/collect_history.h>
#include <monsoon/history/dir/tsdata.h>
#include <monsoon/interpolate.h>
#include <monsoon/objpipe/callback.h>
#include "v2/tsdata.h"

namespace monsoon::history {
namespace {


struct merge_emit_greater_ {
  template<typename X, typename Y>
  auto operator()(const X& x, const Y& y) const
  noexcept
  -> bool {
    return tp(x) > tp(y);
  }

  static auto tp(const time_point& x)
  noexcept
  -> const time_point& {
    return x;
  }

  template<typename T>
  static auto tp(const std::pair<time_point, T>& x)
  noexcept
  -> const time_point& {
    return x.first;
  }

  template<typename... T>
  static auto tp(const std::tuple<time_point, T...>& x)
  noexcept
  -> const time_point& {
    return std::get<0>(x);
  }

  static auto tp(const tsdata& tsd)
  noexcept(noexcept(std::declval<const tsdata&>().time()))
  -> const time_point& {
    return std::get<0>(tsd.time());
  }

  template<typename T>
  static auto tp(const std::shared_ptr<T>& ptr)
  noexcept(noexcept(tp(std::declval<std::add_lvalue_reference_t<std::add_const_t<T>>>())))
  -> const time_point& {
    return tp(*ptr);
  }

  template<typename T>
  static auto tp(const objpipe::detail::adapter_t<T>& pipe)
  -> const time_point& {
    return tp(pipe.front());
  }
};

void merge(time_point& dst, time_point&& src) {
  assert(dst == src);
}

void merge(tsdata::emit_type& dst, tsdata::emit_type&& src) {
  const time_point src_tp = std::get<0>(src);
  const time_point dst_tp = std::get<0>(dst);
  assert(src_tp == dst_tp);

  auto&& src_map = std::get<1>(std::move(src));
  auto& dst_map = std::get<1>(dst);
#if __cplusplus >= 201703
  dst_map.merge(std::move(src_map));
#else
  std::copy(
      std::make_move_iterator(src_map.begin()),
      std::make_move_iterator(src_map.end()),
      std::inserter(dst_map, dst_map.end()));
#endif
}

template<typename ToObjpipeFunctor>
class merge_emit_t {
 private:
  using greater = merge_emit_greater_;
  using unstarted_queue = std::priority_queue<
      std::shared_ptr<const tsdata>,
      std::vector<std::shared_ptr<const tsdata>>,
      greater>;
  using objpipe_type = std::decay_t<decltype(std::declval<const ToObjpipeFunctor&>()(std::declval<const tsdata&>()))>;
  using value_type = typename objpipe_type::value_type;

 public:
  template<typename Iter>
  merge_emit_t(
      Iter files_begin, Iter files_end,
      ToObjpipeFunctor objpipe_fn)
  : unstarted_(files_begin, files_end),
    objpipe_fn_(std::move(objpipe_fn))
  {}

  template<typename Callback>
  auto operator()(Callback cb) -> void {
    while (!unstarted_.empty() || !active_.empty()) {
      // Ensure active has at least one element,
      // and load all unstarted that start at/before active_.front.
      while (active_.empty()
          || (!unstarted_.empty()
              && !greater()(unstarted_.top(), active_.front().front()))) {
        // Convert tsdata to objpipe.
        active_.push_back(objpipe_fn_(*unstarted_.top()));
        unstarted_.pop();

        // Sort new objpipe into active.
        if (active_.back().empty())
          active_.pop_back(); // Discard empty objpipe.
        else
          std::push_heap(active_.begin(), active_.end(), greater());
      }

      // Since empty queues are immediately popped, the loop invariant
      // may break. Compensate here.
      if (active_.empty()) {
        assert(unstarted_.empty());
        break;
      }

      // Read head element of the head of active.
      std::pop_heap(active_.begin(), active_.end(), greater());
      value_type emit = active_.back().pull();
      if (active_.back().empty())
        active_.pop_back();
      else
        std::push_heap(active_.begin(), active_.end(), greater());

      // Merge in all other elements with the same time stamp.
      while (!active_.empty()
          && !greater()(active_.front(), emit)
          && !greater()(emit, active_.front())) {
        std::pop_heap(active_.begin(), active_.end(), greater());
        merge(emit, active_.back().pull());
        if (active_.back().empty())
          active_.pop_back();
        else
          std::push_heap(active_.begin(), active_.end(), greater());
      }

      // Emit merged element.
      cb(std::move(emit));
    }
  }

  auto as_objpipe() &&
  -> decltype(auto) {
    return objpipe::new_callback<value_type>(std::move(*this));
  }

 private:
  unstarted_queue unstarted_;
  ToObjpipeFunctor objpipe_fn_;
  std::vector<objpipe_type> active_;
};

template<typename Iter, typename ToObjpipeFunctor>
auto merge_emit(
    Iter files_begin, Iter files_end,
    ToObjpipeFunctor objpipe_fn)
-> decltype(auto) {
  using impl_type = merge_emit_t<std::decay_t<ToObjpipeFunctor>>;
  return impl_type(files_begin, files_end, std::forward<ToObjpipeFunctor>(objpipe_fn))
      .as_objpipe();
}


namespace interpolation_based {


using timestamped_map = std::unordered_map<
    std::tuple<group_name, metric_name>,
    std::tuple<time_point, metric_value>,
    metric_source::metrics_hash>;
using emit_map_type = std::tuple_element_t<1, tsdata::emit_type>;

/**
 * \brief Update timestamped map with values in emit.
 * \details Overwrites existing values in timestamped_map.
 */
auto update_timestamped_map(
    timestamped_map& timestamps,
    const tsdata::emit_type& emit)
-> void {
  const time_point& tp = std::get<0>(emit);
  const emit_map_type& value_map = std::get<1>(emit);

  for (const auto& value_map_item : value_map) {
    const auto& key = std::get<0>(value_map_item);
    const auto& value = std::get<1>(value_map_item);
    timestamps[key] = std::forward_as_tuple(tp, value);
  }
}

/**
 * \brief Compute the emit for the given time point and stores it in
 * read_ahead.front().
 *
 * \details Updates timestamps in the process.
 * Successive invocations of this function must use ascending \p tp.
 *
 * \param[in] tp The timepoint for which the emit is to be computed.
 * \param[in] slack The amount of look-back and look-forward when interpolating.
 * \param[in,out] timestamps Mapping of timestamped metrics from past data.
 * \param[in,out] read_ahead Queue of read items.
 * \param[in] src The source from which data originates.
 */
template<typename ObjPipe>
auto create_emit_for_tp(
    time_point tp,
    time_point::duration slack,
    timestamped_map& timestamps,
    std::deque<tsdata::emit_type>& read_ahead,
    ObjPipe& src)
-> void {
  // Fill in read-ahead with data up to required data.
  while (!src.empty() && std::get<0>(src.front()) <= tp + slack)
    read_ahead.push_back(src.pull());

  // Use read-ahead data prior to tp, to fill in timestamps.
  while (!read_ahead.empty() && std::get<0>(read_ahead.front()) < tp) {
    update_timestamped_map(timestamps, read_ahead.front());
    read_ahead.pop_front();
  }

  assert(src.empty() || std::get<0>(src.front()) > tp + slack);
  assert(read_ahead.empty() || std::get<0>(read_ahead.front()) >= tp);

  // Pending.front() must contain map at given time point.
  if (std::get<0>(read_ahead.front()) != tp)
    read_ahead.emplace_front(tp, emit_map_type());
}

/**
 * \brief Compute any interpolatable values for the head of the read_ahead queue.
 * \note This returns a copy with the interpolated value.
 * \param[in] slack The amount of look-back and look-forward when interpolating.
 * \param[in] timestamps Mapping of timestamped metrics from past data.
 * \param[in] read_ahead Read ahead queue of data. The interpolation will be done for the front of \p read_ahead.
 */
auto interpolate_for(
    time_point::duration slack,
    const timestamped_map& timestamps,
    const std::deque<tsdata::emit_type>& read_ahead,
    tsdata::emit_type& dst)
-> void {
  assert(!read_ahead.empty());
  const auto tp = std::get<0>(read_ahead.front());
  const auto min_tp = tp - slack;

  std::get<0>(dst) = tp;
  std::get<1>(dst).clear();
  for (const auto& timestamps_elem : timestamps) {
    const auto& key = timestamps_elem.first;
    const auto& predecessor_tp = std::get<0>(timestamps_elem.second);
    const auto& predecessor_value = std::get<1>(timestamps_elem.second);

    if (predecessor_tp < min_tp) continue; // Skip entries that are too old.
    if (std::get<1>(dst).count(key) != 0) continue; // Skip key in dst.

    for (auto iter = read_ahead.begin() + 1, iter_end = read_ahead.end();
        iter != iter_end;
        ++iter) {
      const auto& successor_tp = std::get<0>(*iter);
      const auto& successor_map = std::get<1>(*iter);
      const auto& successor_key_value = successor_map.find(key);

      if (successor_tp > tp + slack) break;
      if (successor_key_value != successor_map.end()) {
        auto opt_value = interpolate(
            tp,
            { predecessor_tp, predecessor_value },
            { successor_tp, successor_key_value->second });
        if (opt_value.has_value())
          std::get<1>(dst).emplace(key, *opt_value);
        break;
      }
    }
  }
}

/**
 * \brief Transformation that interpolates begin and end timestamps.
 * \note Source must be an objpipe reader, aka adapter_t.
 * \tparam Source The objpipe from which values are read.
 */
template<typename Source>
class transformation {
 private:
  using objpipe_errc = objpipe::objpipe_errc;
  using transport_type = objpipe::detail::transport<metric_source::emit_type&&>;

 public:
  transformation(Source&& src,
      const time_range& tr,
      time_point::duration slack)
  : src_(std::move(src)),
    tr_begin(tr.begin()),
    tr_end(tr.end()),
    tr_interval(tr.interval()),
    slack(slack)
  {}

  auto is_pullable() const noexcept
  -> bool {
    return !last_ && (emit_valid || !read_ahead.empty() || src_.is_pullable());
  }

  auto wait() const
  -> objpipe_errc {
    return (is_pullable()
        ? objpipe_errc::success
        : objpipe_errc::closed);
  }

  auto pop_front()
  -> objpipe_errc {
    objpipe_errc e = fill_();
    assert(e != objpipe_errc::success || emit_valid);
    emit_valid = false;
    return e;
  }

  auto front()
  -> transport_type {
    objpipe_errc e = fill_();
    if (e != objpipe_errc::success)
      return transport_type(std::in_place_index<1>, e);

    assert(emit_valid);
    return transport_type(std::in_place_index<0>, std::move(out_value));
  }

  auto front() const
  -> transport_type {
    return const_cast<transformation&>(*this).front();
  }

  auto pull()
  -> transport_type {
    objpipe_errc e = fill_();
    if (e != objpipe_errc::success)
      return transport_type(std::in_place_index<1>, e);

    assert(emit_valid);
    emit_valid = false;
    return transport_type(std::in_place_index<0>, std::move(out_value));
  }

  auto try_pull()
  -> transport_type {
    objpipe_errc e = fill_();
    if (e != objpipe_errc::success)
      return transport_type(std::in_place_index<1>, e);

    assert(emit_valid);
    emit_valid = false;
    return transport_type(std::in_place_index<0>, std::move(out_value));
  }

 private:
  auto fill_()
  -> objpipe_errc {
    if (emit_valid) return objpipe_errc::success;

    if (last_ || (read_ahead.empty() && src_.empty()))
      return objpipe_errc::closed;

    // First emit: fill in initial emit_tp.
    if (std::exchange(first_, false)) {
      assert(read_ahead.empty());
      assert(!src_.empty());
      emit_tp = tr_begin.value_or(std::get<0>(src_.front()));
    }

    if (tr_end.has_value() && emit_tp > *tr_end) {
      last_ = true;
      return objpipe_errc::closed;
    }

    // Ensure variant has correct index.
    if (!std::holds_alternative<tsdata::emit_type>(out_value))
      out_value = tsdata::emit_type();

    // Create pending emit.
    create_emit_for_tp(
        emit_tp,
        slack,
        timestamps,
        read_ahead,
        src_);

    // Interpolate, if emitting tr_begin or tr_end,
    // otherwise, pass through.
    if (emit_tp == tr_begin || emit_tp == tr_end) {
      interpolate_for(slack, timestamps, read_ahead,
          std::get<tsdata::emit_type>(out_value));
      update_timestamped_map(timestamps, read_ahead.front());
      read_ahead.pop_front();
    } else {
      update_timestamped_map(timestamps, read_ahead.front());
      swap(std::get<tsdata::emit_type>(out_value), read_ahead.front());
      read_ahead.pop_front();
    }

    // Record if this is the last value.
    last_ = (emit_tp == tr_end);

    // Update emit_tp for next emit.
    if (tr_interval.has_value())
      emit_tp += *tr_interval;
    else if (!read_ahead.empty())
      emit_tp = std::get<0>(read_ahead.front());
    else if (!src_.empty())
      emit_tp = std::get<0>(src_.front());
    else if (emit_tp == tr_end)
      last_ = true;
    // Clamp emit_tp to be at most tr_end.
    if (tr_end.has_value() && emit_tp > *tr_end)
      emit_tp = *tr_end;

    emit_valid = true;
    return objpipe_errc::success;
  }

  Source src_;

  // Parameters.
  std::optional<time_point> tr_begin, tr_end;
  std::optional<time_point::duration> tr_interval;
  time_point::duration slack;

  // State.
  std::deque<tsdata::emit_type> read_ahead;
  timestamped_map timestamps;
  bool first_ = true, last_ = false;
  time_point emit_tp; ///<\brief Describes the next time point to fill.

  // Output.
  bool emit_valid = false;
  metric_source::emit_type out_value = tsdata::emit_type();
};


} /* namespace monsoon::history::<unnamed>::interpolation_based */


template<typename ObjPipe>
auto interpolation_based_emit(
    objpipe::detail::adapter_t<ObjPipe> src,
    const time_range& tr,
    time_point::duration slack)
-> decltype(auto) {
  using impl_type = interpolation_based::transformation<objpipe::detail::adapter_t<ObjPipe>>;

  return objpipe::detail::adapter(impl_type(
      std::move(src),
      tr, slack));
}


} /* namespace monsoon::history::<unnamed> */


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

auto dirhistory::emit(
    time_range tr,
    path_matcher group_filter,
    tag_matcher tag_filter,
    path_matcher metric_filter,
    time_point::duration slack) const -> objpipe::reader<emit_type> {
  auto tr_begin = tr.begin();
  auto tr_end = tr.end();

  return interpolation_based_emit(
      merge_emit(
          files_.begin(),
          files_.end(),
          [tr_begin, tr_end, group_filter, tag_filter, metric_filter](const tsdata& tsd) {
            return tsd.emit(tr_begin, tr_end, group_filter, tag_filter, metric_filter);
          }),
      tr, slack);
}

auto dirhistory::emit_time(
    time_range tr,
    time_point::duration slack) const -> objpipe::reader<time_point> {
  auto tr_begin = tr.begin();
  auto tr_end = tr.end();

  return merge_emit(
      files_.begin(),
      files_.end(),
      [tr_begin, tr_end](const tsdata& tsd) {
        return tsd.emit_time(tr_begin, tr_end);
      });
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


} /* namespace monsoon::history */
