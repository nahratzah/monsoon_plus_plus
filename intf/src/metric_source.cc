#include <monsoon/metric_source.h>

namespace monsoon {


metric_source::~metric_source() noexcept {}


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
