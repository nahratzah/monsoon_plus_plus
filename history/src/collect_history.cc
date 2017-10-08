#include <monsoon/history/collect_history.h>

namespace monsoon {


collect_history::~collect_history() noexcept {}


std::size_t collect_history::metrics_hash::operator()(
    const std::tuple<group_name, metric_name>& t) const noexcept {
  return std::hash<group_name>()(std::get<0>(t))
      ^ std::hash<metric_name>()(std::get<1>(t));
}

std::size_t collect_history::metrics_hash::operator()(
    const std::tuple<simple_group, metric_name>& t) const noexcept {
  return std::hash<simple_group>()(std::get<0>(t))
      ^ std::hash<metric_name>()(std::get<1>(t));
}


} /* namespace monsoon */
