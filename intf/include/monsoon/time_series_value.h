#ifndef MONSOON_TIME_SERIES_VALUE_H
#define MONSOON_TIME_SERIES_VALUE_H

#include <monsoon/intf_export_.h>
#include <monsoon/group_name.h>
#include <monsoon/metric_name.h>
#include <monsoon/metric_value.h>
#include <cstddef>
#include <functional>
#include <unordered_map>
#include <initializer_list>
#include <optional>

namespace monsoon {


class monsoon_intf_export_ time_series_value {
 public:
  using metric_map = std::unordered_map<metric_name, metric_value>;

  time_series_value() = default;
  time_series_value(const time_series_value&) = default;
  time_series_value(time_series_value&&) noexcept;
  time_series_value& operator=(const time_series_value&) = default;
  time_series_value& operator=(time_series_value&&) noexcept;

  template<typename Iter> time_series_value(group_name, Iter, Iter);
  time_series_value(group_name, metric_map) noexcept;
  time_series_value(group_name, std::initializer_list<metric_map::value_type>);

  const group_name& get_name() const noexcept;
  const metric_map& get_metrics() const noexcept;
  auto operator[](const metric_name&) const noexcept
      -> std::optional<metric_value>;

  bool operator==(const time_series_value&) const noexcept;
  bool operator!=(const time_series_value&) const noexcept;

 private:
  group_name name_;
  metric_map metrics_;
};


} /* namespace monsoon */


namespace std {


template<>
struct hash<monsoon::time_series_value> {
  using argument_type = const monsoon::time_series_value&;
  using result_type = size_t;

  monsoon_intf_export_
  size_t operator()(const monsoon::time_series_value&) const noexcept;
};


} /* namespace std */

#include "time_series_value-inl.h"

#endif /* MONSOON_TIME_SERIES_VALUE_H */
