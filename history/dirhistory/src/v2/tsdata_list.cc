#include "tsdata_list.h"
#include "../raw_file_segment_writer.h"

namespace monsoon {
namespace history {
namespace v2 {


tsdata_v2_list::tsdata_v2_list(file_segment<tsdata_list>&& data,
    const tsdata_v2::carg& constructor_arg)
: tsdata_v2(constructor_arg),
  data_(std::move(data))
{}

tsdata_v2_list::~tsdata_v2_list() noexcept {}

std::shared_ptr<io::fd> tsdata_v2_list::fd() const noexcept {
  return data_.ctx().fd();
}

bool tsdata_v2_list::is_writable() const noexcept {
  return fd()->can_write();
}

std::vector<time_series> tsdata_v2_list::read_all_raw_() const {
  std::vector<time_series> result;
  visit(
      [&result](const time_point& ts, std::shared_ptr<const tsdata_list::record_array> records) {
        time_series::tsv_set tsv_set;
        std::transform(records->begin(), records->end(),
            std::inserter(tsv_set, tsv_set.end()),
            [](const auto& group_entry) {
              return time_series_value(
                  std::get<0>(group_entry), // Group name
                  *std::get<1>(group_entry).get()); // Metrics map
            });
        result.push_back(time_series(ts, std::move(tsv_set)));
      });
  return result;
}

void tsdata_v2_list::push_back(const time_series& ts) {
  encdec_writer out = encdec_writer(data_.ctx(), hdr_file_size());

  dictionary_delta dict;
  std::optional<file_segment_ptr> tsdata_pred;
  if (data_.file_ptr().offset() != 0u) {
    dict = *data_.get()->dictionary();
    tsdata_pred = data_.file_ptr();
  }
  assert(!dict.update_pending());

  const file_segment_ptr tsfile_ptr =
      encode_tsdata(out, ts, std::move(dict), std::move(tsdata_pred));

  out.ctx().fd()->flush();
  update_hdr(ts.get_time(), ts.get_time(), tsfile_ptr, out.offset());
  data_.update_addr(tsfile_ptr);
}

void tsdata_v2_list::emit(
    const std::function<void(time_point, emit_map&&)>& accept_fn,
    std::optional<time_point> tr_begin, std::optional<time_point> tr_end,
    const std::function<bool(const group_name&)>& group_filter,
    const std::function<bool(const group_name&, const metric_name&)>& filter) const {
  using std::swap;

  // Reorder map, used to deal with non-distinct/non-sorted file.
  // The reorder map contains:
  // - 0 elements, if is_sorted() && is_distinct()
  // - 1 elements, if is_sorted() && !is_distinct()
  // - * elements, if !is_sorted()
  std::vector<std::tuple<time_point, emit_map>> reorder_map;

  visit(
      [&](
          const time_point& ts,
          std::shared_ptr<const tsdata_list::record_array> records) {
        if ((tr_begin.has_value() && tr_begin.value() > ts)
            || (tr_end.has_value() && tr_end.value() < ts))
          return; /* SKIP */

        emit_map emit_data;

        std::for_each(records->begin(), records->end(),
            [&emit_data, &group_filter, &filter](const auto& group_entry) {
              const auto& group_name = std::get<0>(group_entry);

              if (group_filter(group_name)) {
                const auto metrics = std::get<1>(group_entry).get();
                std::for_each(metrics->begin(), metrics->end(),
                    [&group_name, &emit_data, &filter](
                        const auto& metric_entry) {
                      const auto& metric_name = std::get<0>(metric_entry);
                      const auto& metric_value = std::get<1>(metric_entry);

                      if (filter(group_name, metric_name)) {
                        emit_data.emplace(
                            std::piecewise_construct,
                            std::forward_as_tuple(group_name, metric_name),
                            std::forward_as_tuple(metric_value));
                      }
                    });
              }
            });

        if (!is_sorted()) { // Collect everything and bulk handle at end.
          reorder_map.emplace_back(ts, std::move(emit_data));
        } else if (!is_distinct()) {
          // Sorted, but not distinct. Keep trailing element in reorder_map
          // and emit/merge appropriately.
          if (reorder_map.empty()) {
            reorder_map.emplace_back(ts, std::move(emit_data));
          } else {
            assert(reorder_map.size() == 1u);
            auto& reorder_ts = std::get<0>(reorder_map.back());
            auto& reorder_data = std::get<1>(reorder_map.back());

            if (reorder_ts == ts) {
              swap(reorder_data, emit_data); // Keep last emit.
#if __cplusplus >= 201703
              if (reorder_data.get_allocator() == emit_data.get_allocator())
                reorder_data.merge(std::move(emit_data));
              else
#endif
                reorder_data.insert(
                  std::make_move_iterator(emit_data.begin()),
                  std::make_move_iterator(emit_data.end()));
            } else {
              // Emit old data, now that it is cleaned up.
              accept_fn(
                  reorder_ts,
                  std::move(reorder_data));
              // Store new data into reorder_map.
              std::tie(reorder_ts, reorder_data) =
                  std::forward_as_tuple(ts, std::move(emit_data));
            }
          }
        } else {
          // Sorted and unique, emit directly.
          accept_fn(ts, std::move(emit_data));
        }
      });

  if (!is_sorted()) {
    // Stable sort, to maintain encounter order.
    std::stable_sort(reorder_map.begin(), reorder_map.end(),
        [](const auto& x, const auto& y) {
          return std::get<0>(x) < std::get<0>(y);
        });
    // Reverse, so we can use it as a stack.
    // NOTE: we can't use > in the sort function, as that would
    // not reverse encounter order.
    std::reverse(reorder_map.begin(), reorder_map.end());
  } else {
    assert(reorder_map.size() <= 1u);
  }

  while (!reorder_map.empty()) {
    time_point ts;
    emit_map emit_data;
    std::tie(ts, emit_data) = std::move(reorder_map.back());
    reorder_map.pop_back();

    if (!is_distinct()) {
      while (!reorder_map.empty() && std::get<0>(reorder_map.back()) == ts) {
        // Keep later encountered data before earlier data,
        // so that stable_sort, unique can erase overwritten data.
        // NOTE: swap followed by insert-at-end
        // is effectively the same as intert-at-begin.
        swap(emit_data, std::get<1>(reorder_map.back()));
#if __cplusplus >= 201703
        if (emit_data.get_allocator() == reorder_map.back().get_allocator())
          emit_data.merge(std::move(reorder_map.back()));
        else
#endif
          emit_data.insert(
              std::make_move_iterator(std::get<1>(reorder_map.back()).begin()),
              std::make_move_iterator(std::get<1>(reorder_map.back()).end()));
        reorder_map.pop_back();
      }
    }

    accept_fn(ts, std::move(emit_data));
  }
}

void tsdata_v2_list::emit_time(
    const std::function<void(time_point)>& accept_fn,
    std::optional<time_point> tr_begin, std::optional<time_point> tr_end) const {
  std::vector<time_point> reorder_map;
  visit(
      [&](
          const time_point& ts,
          const std::shared_ptr<const tsdata_list::record_array>& records) {
        if (!is_sorted()) {
          reorder_map.push_back(ts);
        } else if (!is_distinct()) {
          if (reorder_map.empty()) {
            reorder_map.push_back(ts);
          } else if (reorder_map.back() == ts) {
            /* SKIP */
          } else {
            accept_fn(reorder_map.back());
            reorder_map.back() = ts;
          }
        } else {
          // Sorted and unique, emit directly.
          accept_fn(ts);
        }
      });

  if (!is_sorted()) {
    std::sort(reorder_map.begin(), reorder_map.end());
    if (!is_distinct()) {
      reorder_map.erase(
          std::unique(reorder_map.begin(), reorder_map.end()),
          reorder_map.end());
    }
  }

  std::for_each(reorder_map.begin(), reorder_map.end(), std::ref(accept_fn));
}


}}} /* namespace monsoon::history::v2 */
