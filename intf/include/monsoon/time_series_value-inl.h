#ifndef MONSOON_TIME_SERIES_VALUE_INL_H
#define MONSOON_TIME_SERIES_VALUE_INL_H

#include <utility>

namespace monsoon {


inline time_series_value::time_series_value(time_series_value&& other) noexcept
: name_(std::move(other.name_)),
  metrics_(std::move(other.metrics_))
{}

inline auto time_series_value::operator=(time_series_value&& other) noexcept
->  time_series_value& {
  name_ = std::move(other.name_);
  metrics_ = std::move(other.metrics_);
  return *this;
}

template<typename Iter>
time_series_value::time_series_value(group_name name, Iter b, Iter e)
: name_(std::move(name)),
  metrics_(b, e)
{}

inline time_series_value::time_series_value(group_name name,
    metric_map metrics) noexcept
: name_(std::move(name)),
  metrics_(std::move(metrics))
{}

inline time_series_value::time_series_value(group_name name,
    std::initializer_list<metric_map::value_type> metrics)
: name_(std::move(name)),
  metrics_(metrics)
{}

inline auto time_series_value::get_name() const noexcept -> const group_name& {
  return name_;
}

inline auto time_series_value::get_metrics() const noexcept
->  const metric_map& {
  return metrics_;
}

inline auto time_series_value::metrics() noexcept
->  metric_map& {
  return metrics_;
}

inline auto time_series_value::operator!=(const time_series_value& other)
    const noexcept
->  bool {
  return !(*this == other);
}


} /* namespace monsoon */

#endif /* MONSOON_TIME_SERIES_VALUE_INL_H */
