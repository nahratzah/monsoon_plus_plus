#ifndef MONSOON_METRIC_VALUE_H
#define MONSOON_METRIC_VALUE_H

#include <cstdint>
#include <functional>
#include <string>
#include <monsoon/optional.h>
#include <monsoon/any.h>
#include <monsoon/histogram.h>
#include <iosfwd>

namespace monsoon {


class metric_value {
 public:
  using unsigned_type = std::uint64_t;
  using signed_type = std::int64_t;
  using fp_type = std::double_t;
  using types =
      any<bool, signed_type, unsigned_type, fp_type, std::string, histogram>;

  constexpr metric_value() noexcept = default;
  metric_value(const metric_value&) = default;
  metric_value(metric_value&&) noexcept;
  metric_value& operator=(const metric_value&) = default;
  metric_value& operator=(metric_value&&) noexcept;

  explicit metric_value(bool) noexcept;
  explicit metric_value(signed_type) noexcept;
  explicit metric_value(unsigned_type) noexcept;
  explicit metric_value(fp_type) noexcept;
  explicit metric_value(std::string) noexcept;
  explicit metric_value(histogram) noexcept;

  bool operator==(const metric_value&) const noexcept;
  bool operator!=(const metric_value&) const noexcept;

  const optional<types>& get() const noexcept;

  optional<bool> as_bool() const noexcept;
  optional<any<signed_type, unsigned_type, fp_type>> as_number()
      const noexcept;
  optional<any<signed_type, unsigned_type, fp_type, histogram>>
      as_number_or_histogram() const noexcept;
  optional<std::string> as_string() const;

  static bool before(const metric_value&, const metric_value&) noexcept;

 private:
  optional<types> value_;
};

metric_value operator!(const metric_value&) noexcept;
metric_value operator&&(const metric_value&, const metric_value&) noexcept;
metric_value operator||(const metric_value&, const metric_value&) noexcept;
metric_value operator-(const metric_value&) noexcept;
metric_value operator+(const metric_value&, const metric_value&) noexcept;
metric_value operator-(const metric_value&, const metric_value&) noexcept;
metric_value operator*(const metric_value&, const metric_value&) noexcept;
metric_value operator/(const metric_value&, const metric_value&) noexcept;
metric_value operator%(const metric_value&, const metric_value&) noexcept;

std::ostream& operator<<(std::ostream&, const metric_value&);

metric_value equal(const metric_value&, const metric_value&) noexcept;
metric_value unequal(const metric_value&, const metric_value&) noexcept;
metric_value less(const metric_value&, const metric_value&) noexcept;
metric_value greater(const metric_value&, const metric_value&) noexcept;
metric_value less_equal(const metric_value&, const metric_value&) noexcept;
metric_value greater_equal(const metric_value&, const metric_value&) noexcept;


} /* namespace monsoon */


namespace std {


template<>
struct hash<monsoon::metric_value> {
  using argument_type = const monsoon::metric_value&;
  using result_type = size_t;

  size_t operator()(const monsoon::metric_value&) const noexcept;
};

std::string to_string(const monsoon::metric_value&);


} /* namespace std */

#include "metric_value-inl.h"

#endif /* MONSOON_METRIC_VALUE_H */
