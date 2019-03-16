#include <monsoon/history/collect_history.h>
#include <objpipe/of.h>

namespace monsoon {
namespace {


auto make_time_series_(const metric_source::metric_emit& c) -> time_series {
  std::unordered_map<group_name, time_series_value> tsv_map;
  const time_point tp = std::get<0>(c);
  std::for_each(
      std::get<1>(c).begin(),
      std::get<1>(c).end(),
      [&tsv_map](const auto& e) {
        const auto& group_name = std::get<0>(e.first);
        const auto& metric_name = std::get<1>(e.first);
        const auto& metric_value = e.second;

#if __cplusplus >= 201703
        time_series_value& tsv =
            tsv_map.try_emplace(group_name, group_name).first->second;
#else
        time_series_value& tsv = tsv_map.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(group_name),
            std::forward_as_tuple(group_name)
            ).first->second;
#endif
        tsv.metrics()[metric_name] = metric_value;
      });
  auto ts_elems = objpipe::of(std::move(tsv_map))
      .iterate()
      .select_second();

  return time_series(tp, ts_elems.begin(), ts_elems.end());
}


} /* namespace monsoon::<unnamed> */


collect_history::~collect_history() noexcept = default;

auto collect_history::push_back(const metric_emit& m) -> void {
  if (!std::get<1>(m).empty()) do_push_back_(m);
}

auto collect_history::do_push_back_(const metric_emit& m) -> void {
  push_back(make_time_series_(m));
}


} /* namespace monsoon */
