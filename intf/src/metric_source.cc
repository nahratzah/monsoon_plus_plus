#include <monsoon/metric_source.h>

namespace monsoon {


metric_source::~metric_source() noexcept {}

void metric_source::emit(
    acceptor<group_name, metric_name, metric_value>& accept_fn,
    time_range tr,
    std::function<bool(const simple_group&)> group_filter,
    std::function<bool(const simple_group&, const metric_name&)> metric_filter,
    time_point::duration slack) const {
  std::function<bool(const group_name&)> wrapped_group_filter =
      [&group_filter](const group_name& gname) {
        return group_filter(gname.get_path());
      };
  std::function<bool(const group_name&, const metric_name&)> wrapped_metric_filter =
      [&metric_filter](const group_name& gname, const metric_name& mname) {
        return metric_filter(gname.get_path(), mname);
      };

  return emit(
      accept_fn,
      std::move(tr),
      std::move(wrapped_group_filter),
      std::move(wrapped_metric_filter),
      std::move(slack));
}


std::size_t metric_source::metrics_hash::operator()(
    const std::tuple<group_name, metric_name>& t) const noexcept {
  return std::hash<group_name>()(std::get<0>(t))
      ^ std::hash<metric_name>()(std::get<1>(t));
}

std::size_t metric_source::metrics_hash::operator()(
    const std::tuple<simple_group, metric_name>& t) const noexcept {
  return std::hash<simple_group>()(std::get<0>(t))
      ^ std::hash<metric_name>()(std::get<1>(t));
}


} /* namespace monsoon */
