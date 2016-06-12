#ifndef MONSOON_TIME_SERIES_VALUE_H
#define MONSOON_TIME_SERIES_VALUE_H

#include <monsoon/group_name.h>
#include <monsoon/metric_name.h>
#include <monsoon/metric_value.h>
#include <chrono>
#include <cstddef>
#include <functional>
#include <unordered_map>

namespace monsoon {


class time_series_value {
 public:
  using metric_map = std::unordered_map<metric_name, metric_value>;
  using time_point = std::chrono::time_point<std::chrono::system_clock,
                                             std::chrono::milliseconds>;

  time_series_value() = default;
  time_series_value(const time_series_value&) = default;
  time_series_value(time_series_value&&) noexcept;
  time_series_value& operator=(const time_series_value&) = default;
  time_series_value& operator=(time_series_value&&) noexcept;

  template<typename Iter> time_series_value(time_point, group_name,
                                            Iter, Iter);

  const time_point& get_time() const noexcept;
  const group_name& get_name() const noexcept;
  const metric_map& get_metrics() const noexcept;
  optional<const metric_value&> operator[](const metric_name&) const noexcept;

  bool operator==(const time_series_value&) const noexcept;
  bool operator!=(const time_series_value&) const noexcept;

 private:
  time_point tp_;
  group_name name_;
  metric_map metrics_;
};


} /* namespace monsoon */


namespace std {


template<>
struct hash<monsoon::time_series_value> {
  using argument_type = const monsoon::time_series_value&;
  using result_type = size_t;

  size_t operator()(const monsoon::time_series_value&) const noexcept;
};


} /* namespace std */

#include "time_series_value-inl.h"

#endif /* MONSOON_TIME_SERIES_VALUE_H */
