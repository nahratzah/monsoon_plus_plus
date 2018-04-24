#ifndef V2_TSDATA_LIST_INL_H
#define V2_TSDATA_LIST_INL_H

#include <iterator>
#include <stack>
#include <utility>
#include <vector>

namespace monsoon {
namespace history {
namespace v2 {


template<typename Fn>
void tsdata_v2_list::visit(Fn fn) const {
  if (data_.file_ptr().offset() == 0u) // Empty file, bail out early
    return;

  // Build stack of tsdata_list entries.
  std::stack<std::shared_ptr<const tsdata_list>> data_stack;
  data_stack.push(data_.get());
  const auto dict_ptr = data_stack.top()->dictionary();
  for (auto pred_ptr = data_stack.top()->pred();
      pred_ptr != nullptr;
      pred_ptr = data_stack.top()->pred()) {
    data_stack.push(pred_ptr);
  }

  // Process stack of tsdata_list entries.
  while (!data_stack.empty()) {
    const auto& dptr = data_stack.top();
    std::invoke(fn, dptr->ts(), dptr->records(*dict_ptr));
    data_stack.pop();
  }
}

template<typename Callback>
auto tsdata_v2_list::direct_emit_cb_(
    std::optional<time_point> begin,
    std::optional<time_point> end,
    const path_matcher& group_filter,
    const tag_matcher& tag_filter,
    const path_matcher& metric_filter,
    Callback cb) const
-> void {
  // We keep an emit record outside the set, so we can re-use the memory
  // allocated for the map.
  emit_type emit_data;

  visit(
      [&](const time_point& tp, std::shared_ptr<const tsdata_list::record_array> records) {
        if ((begin.has_value() && tp < *begin)
            || (end.has_value() && tp > *end))
          return; // Skip

        std::get<0>(emit_data) = tp;
        std::get<1>(emit_data).clear();

        for (const auto& record : *records) {
          const auto& group_name = std::get<0>(record);
          const auto& group_data = std::get<1>(record);

          if (group_filter(group_name.get_path())
              && tag_filter(group_name.get_tags())) {
            for (const auto& [metric_name, metric_value] : *group_data.get()) {
              if (metric_filter(metric_name)) {
                auto ins_pos = std::get<1>(emit_data).emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(group_name, metric_name),
                    std::forward_as_tuple(metric_value));
                if (!ins_pos.second)
                  ins_pos.first->second = metric_value;
              }
            }
          }
        }

        cb(std::move(emit_data));
      });
}

template<typename Callback>
auto tsdata_v2_list::emit_cb_sorted_with_duplicates_(
    std::optional<time_point> begin,
    std::optional<time_point> end,
    const path_matcher& group_filter,
    const tag_matcher& tag_filter,
    const path_matcher& metric_filter,
    Callback cb) const
-> void {
  // Since the data is ordered, but not distinct, we only need to
  // execute the equivalent of merging adjecent equal values.
  using std::swap;

  emit_type predecessor; // Pending emit, if first != true.
  bool first = true; // Indicates that the first emit is still to happen.

  direct_emit_cb_(
      begin, end, group_filter, tag_filter, metric_filter,
      [&predecessor, &first, &cb](emit_type&& data) {
        if (std::exchange(first, false)) {
          predecessor = std::move(data);
          return;
        }

        if (std::get<0>(predecessor) != std::get<0>(data)) {
          cb(std::move(predecessor));
          // Use swap instead of move assignment, so that direct_emit_cb_
          // can reuse any remaining storage in the predecessor.
          swap(predecessor, data);
          return;
        }

#if __cplusplus >= 201703
        // Merge predecessor into data (so that later emit wins).
        std::get<1>(data).merge(std::get<1>(std::move(predecessor)));
#else
        // Merge predecessor into data (so that later emit wins).
        std::copy(
            std::make_move_iterator(std::get<1>(predecessor).begin()),
            std::make_move_iterator(std::get<1>(predecessor).end()),
            std::inserter(std::get<1>(data), std::get<1>(data).end()));
#endif
        // Swap predecessor and data, so that merged data is available in predecessor.
        swap(std::get<1>(predecessor), std::get<1>(data));
      });

  if (!first)
    cb(std::move(predecessor));
}

template<typename Callback>
auto tsdata_v2_list::emit_cb_unsorted_(
    std::optional<time_point> begin,
    std::optional<time_point> end,
    const path_matcher& group_filter,
    const tag_matcher& tag_filter,
    const path_matcher& metric_filter,
    Callback cb) const
-> void {
  std::vector<emit_type> data;
  direct_emit_cb_(
      begin, end, group_filter, tag_filter, metric_filter,
      [&data](emit_type&& emit) {
        data.push_back(std::move(emit));
      });

  std::stable_sort(data.begin(), data.end(),
      [](const emit_type& x, const emit_type& y) {
        return std::get<0>(x) < std::get<0>(y);
      });

  for (auto iter = data.begin(), iter_end = data.end();
      iter != iter_end;
      ++iter) {
    auto successor = std::next(iter);
    if (successor == data.end()
        || std::get<0>(*iter) != std::get<0>(*successor)) {
      cb(std::move(*iter));
      continue;
    }

    // Skip iter and instead merge its elements into successor.
#if __cplusplus >= 201703
    successor.second.merge(std::move(iter->second));
#else
    std::copy(
        std::make_move_iterator(std::get<1>(*iter).begin()),
        std::make_move_iterator(std::get<1>(*iter).end()),
        std::inserter(std::get<1>(*successor), std::get<1>(*successor).end()));
#endif
  }
}


}}} /* namespace monsoon::history::v2 */

#endif /* V2_TSDATA_LIST_INL_H */
