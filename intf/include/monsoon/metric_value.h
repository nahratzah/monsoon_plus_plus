#ifndef MONSOON_METRIC_VALUE_H
#define MONSOON_METRIC_VALUE_H

#include <functional>
#include <string>
#include <monsoon/optional.h>
#include <monsoon/any.h>

namespace monsoon {


class metric_value {
 public:
  using types = any<bool, long, unsigned long, double, std::string>;

  constexpr metric_value() noexcept = default;
  metric_value(const metric_value&) = default;
  metric_value(metric_value&&) noexcept;
  metric_value& operator=(const metric_value&) = default;
  metric_value& operator=(metric_value&&) noexcept;

  explicit metric_value(bool) noexcept;
  explicit metric_value(long) noexcept;
  explicit metric_value(unsigned long) noexcept;
  explicit metric_value(double) noexcept;
  explicit metric_value(std::string) noexcept;

  bool operator==(const metric_value&) const noexcept;
  bool operator!=(const metric_value&) const noexcept;

  const optional<types>& get() const noexcept;

  optional<any<long, unsigned long, double>> as_number() const noexcept;

 private:
  optional<types> value_;
};

metric_value operator+(const metric_value&, const metric_value&) noexcept;
metric_value operator-(const metric_value&, const metric_value&) noexcept;
metric_value operator*(const metric_value&, const metric_value&) noexcept;
metric_value operator/(const metric_value&, const metric_value&) noexcept;
metric_value operator%(const metric_value&, const metric_value&) noexcept;


} /* namespace monsoon */


namespace std {


template<>
struct hash<monsoon::metric_value> {
  using argument_type = const monsoon::metric_value&;
  using result_type = size_t;

  size_t operator()(const monsoon::metric_value&) const noexcept;
};


} /* namespace std */

#include "metric_value-inl.h"

#endif /* MONSOON_METRIC_VALUE_H */
