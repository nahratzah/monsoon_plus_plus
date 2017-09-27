#ifndef MONSOON_METRIC_VALUE_H
#define MONSOON_METRIC_VALUE_H

#include <monsoon/intf_export_.h>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <monsoon/histogram.h>
#include <iosfwd>
#include <type_traits>
#include <optional>
#include <variant>

namespace monsoon {


class monsoon_intf_export_ metric_value {
 public:
  struct empty {
    constexpr empty() noexcept {}
    constexpr empty(const empty&) noexcept {}
    constexpr empty(empty&&) noexcept {}
    constexpr empty& operator=(const empty&) noexcept { return *this; }
    constexpr empty& operator=(empty&&) noexcept { return *this; }
    constexpr bool operator==(const empty&) const noexcept { return true; }
    constexpr bool operator!=(const empty&) const noexcept { return false; }
  };
  using unsigned_type = std::uint64_t;
  using signed_type = std::int64_t;
  using fp_type = std::double_t;

  static_assert(std::is_copy_constructible_v<empty>
      && std::is_move_constructible_v<empty>,
      "empty type constructors are borked.");
  static_assert(std::is_copy_constructible_v<bool>
      && std::is_move_constructible_v<bool>,
      "bool type constructors are borked.");
  static_assert(std::is_copy_constructible_v<signed_type>
      && std::is_move_constructible_v<signed_type>,
      "signed_type type constructors are borked.");
  static_assert(std::is_copy_constructible_v<unsigned_type>
      && std::is_move_constructible_v<unsigned_type>,
      "unsigned_type type constructors are borked.");
  static_assert(std::is_copy_constructible_v<fp_type> &&
      std::is_move_constructible_v<fp_type>,
      "fp_type type constructors are borked.");
  static_assert(std::is_copy_constructible_v<std::string>
      && std::is_move_constructible_v<std::string>,
      "std::string type constructors are borked.");
  static_assert(std::is_copy_constructible_v<histogram>
      && std::is_move_constructible_v<histogram>,
      "histogram type constructors are borked.");

  static_assert(std::is_copy_assignable_v<empty> &&
      std::is_move_assignable_v<empty>,
      "empty type assignment is borked.");
  static_assert(std::is_copy_assignable_v<bool> &&
      std::is_move_assignable_v<bool>,
      "bool type assignment is borked.");
  static_assert(std::is_copy_assignable_v<signed_type> &&
      std::is_move_assignable_v<signed_type>,
      "signed_type type assignment is borked.");
  static_assert(std::is_copy_assignable_v<unsigned_type> &&
      std::is_move_assignable_v<unsigned_type>,
      "unsigned_type type assignment is borked.");
  static_assert(std::is_copy_assignable_v<fp_type> &&
      std::is_move_assignable_v<fp_type>,
      "fp_type type assignment is borked.");
  static_assert(std::is_copy_assignable_v<std::string> &&
      std::is_move_assignable_v<std::string>,
      "std::string type assignment is borked.");
  static_assert(std::is_copy_assignable_v<histogram> &&
      std::is_move_assignable_v<histogram>,
      "histogram type assignment is borked.");

  using types = std::variant<
      empty, bool, signed_type, unsigned_type, fp_type, std::string, histogram>;

  constexpr metric_value() noexcept = default;
  metric_value(const metric_value&) = default;
  metric_value(metric_value&&) noexcept;
  metric_value& operator=(const metric_value&) = default;
  metric_value& operator=(metric_value&&) noexcept;

  explicit metric_value(bool) noexcept;
  explicit metric_value(fp_type) noexcept;
  explicit metric_value(std::string_view) noexcept;
  explicit metric_value(const char*) noexcept;
  explicit metric_value(histogram) noexcept;

  // signed type
  template<typename T,
      typename =
          std::enable_if_t<
                 std::is_integral<T>()
              && !std::is_same<bool, T>()
              && !std::is_same<char, T>()
              && !std::is_same<char16_t, T>()
              && !std::is_same<char32_t, T>()
              && !std::is_same<wchar_t, T>(),
              void
          >>
  explicit metric_value(const T&) noexcept;

  bool operator==(const metric_value&) const noexcept;
  bool operator!=(const metric_value&) const noexcept;

  const types& get() const noexcept;

  std::optional<bool> as_bool() const noexcept;
  std::optional<std::variant<signed_type, unsigned_type, fp_type>> as_number()
      const noexcept;
  std::optional<std::variant<signed_type, unsigned_type, fp_type, histogram>>
      as_number_or_histogram() const noexcept;
  std::optional<std::string> as_string() const;

  static bool before(const metric_value&, const metric_value&) noexcept;

 private:
  types value_;
};

monsoon_intf_export_
metric_value operator!(const metric_value&) noexcept;
monsoon_intf_export_
metric_value operator&&(const metric_value&, const metric_value&) noexcept;
monsoon_intf_export_
metric_value operator||(const metric_value&, const metric_value&) noexcept;
monsoon_intf_export_
metric_value operator-(const metric_value&) noexcept;
monsoon_intf_export_
metric_value operator+(const metric_value&, const metric_value&) noexcept;
monsoon_intf_export_
metric_value operator-(const metric_value&, const metric_value&) noexcept;
monsoon_intf_export_
metric_value operator*(const metric_value&, const metric_value&) noexcept;
monsoon_intf_export_
metric_value operator/(const metric_value&, const metric_value&) noexcept;
monsoon_intf_export_
metric_value operator%(const metric_value&, const metric_value&) noexcept;

monsoon_intf_export_
std::ostream& operator<<(std::ostream&, const metric_value&);

monsoon_intf_export_
metric_value equal(const metric_value&, const metric_value&) noexcept;
monsoon_intf_export_
metric_value unequal(const metric_value&, const metric_value&) noexcept;
monsoon_intf_export_
metric_value less(const metric_value&, const metric_value&) noexcept;
monsoon_intf_export_
metric_value greater(const metric_value&, const metric_value&) noexcept;
monsoon_intf_export_
metric_value less_equal(const metric_value&, const metric_value&) noexcept;
monsoon_intf_export_
metric_value greater_equal(const metric_value&, const metric_value&) noexcept;

monsoon_intf_export_
std::string to_string(const monsoon::metric_value&);


} /* namespace monsoon */


namespace std {


template<>
struct hash<monsoon::metric_value::empty> {
  using argument_type = const monsoon::metric_value::empty&;
  using result_type = size_t;

  size_t operator()(argument_type) const noexcept { return 0; }
};

template<>
struct hash<monsoon::metric_value> {
  using argument_type = const monsoon::metric_value&;
  using result_type = size_t;

  monsoon_intf_export_
  size_t operator()(const monsoon::metric_value&) const noexcept;
};


} /* namespace std */

#include "metric_value-inl.h"

#endif /* MONSOON_METRIC_VALUE_H */
