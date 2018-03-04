#ifndef MONSOON_METRIC_VALUE_H
#define MONSOON_METRIC_VALUE_H

///\file
///\ingroup intf

#include <monsoon/intf_export_.h>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <monsoon/histogram.h>
#include <monsoon/cache/allocator.h>
#include <iosfwd>
#include <type_traits>
#include <optional>
#include <variant>

namespace monsoon {


/**
 * \brief Metric value.
 * \ingroup intf
 *
 * \details A metric value is either:
 * \li <em>\ref monsoon::metric_value::empty</em> representing an empty value.
 * \li <em>a boolean</em> value representing true or false.
 * \li <em>a scalar</em> representing an integral or floating point value.
 * \li <em>a \ref monsoon::histogram "histogram"</em>
 * \li <em>a std::string</em> representing a textual value.
 */
class monsoon_intf_export_ metric_value {
 public:
  /**
   * \brief The empty value.
   *
   * An empty metric value is defined as existing, but holds no meaningful information.
   * \todo Maybe we should get rid of this and use std::optional<metric_value> where appropriate instead?
   */
  struct empty {
    ///\brief Equality.
    constexpr bool operator==(const empty&) const noexcept { return true; }
    ///\brief Inequality.
    constexpr bool operator!=(const empty&) const noexcept { return false; }
  };
  /**
   * \brief The unsigned integral scalar.
   *
   * This value is used for integral values that are not negative.
   */
  using unsigned_type = std::uint64_t;
  /**
   * \brief The signed integral scalar.
   *
   * This value is only used for integral values that are negative.
   */
  using signed_type = std::int64_t;
  /**
   * \brief The floating point scalar.
   */
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

  using string_type = std::basic_string<
      char,
      std::char_traits<char>,
      cache_allocator<std::allocator<char>>>;
  using string_ptr = std::shared_ptr<string_type>;

  /**
   * \brief Underlying variant holding any one of the permitted values.
   */
  using types = std::variant<
      empty, bool, signed_type, unsigned_type, fp_type, std::string_view, histogram>;

 private:
  using internal_types = std::variant<
      empty, bool, signed_type, unsigned_type, fp_type, string_ptr, histogram>;

 public:
  ///@{
  /**
   * \brief Default constructor creates an \ref empty metric value.
   */
  constexpr metric_value() noexcept = default;
  metric_value(const metric_value&);
  metric_value(metric_value&&) noexcept;
  metric_value& operator=(const metric_value&);
  metric_value& operator=(metric_value&&) noexcept;
  ~metric_value() noexcept;

  /**
   * \brief Create a metric value from a boolean.
   */
  explicit metric_value(bool) noexcept;
  /**
   * \brief Create a metric value from a floating point type.
   */
  explicit metric_value(fp_type) noexcept;
  /**
   * \brief Create a metric value from a string.
   */
  explicit metric_value(std::string_view) noexcept;
  /**
   * \brief Create a metric value from a string.
   */
  explicit metric_value(const char*) noexcept;
  /**
   * \brief Create a metric value from a \ref histogram.
   */
  explicit metric_value(histogram) noexcept;

  /**
   * \brief Create a metric value from an integral.
   */
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
  ///@}

  ///@{
  ///\brief Test if two metric values hold the same type and value.
  bool operator==(const metric_value&) const noexcept;
  ///\brief Test if two metric values do not hold the same type and value.
  bool operator!=(const metric_value&) const noexcept;
  ///@}

  /**
   * \brief Parse metric value expression.
   * \param s A string representation of an expression.
   * \return A metric value, as represented by the expression.
   * \throw std::invalid_argument indicating \p s does not hold a valid expression.
   */
  static metric_value parse(std::string_view s);

  ///\brief Retrieve underlying variant.
  types get() const noexcept;

  ///@{
  /**
   * \brief Interpret the metric value as a boolean.
   *
   * \return
   * \li If the metric value holds a boolean, the underlying boolean is returned unmodified.
   * \li If the metric value holds a histogram, the returned boolean represents if the histogram holds values (i.e., is not empty).
   * \li If the metric value holds a numeric type, the returned boolean is true iff the numeric type is non zero.
   * \li Otherwise, an empty optional is returned.
   *
   * \note string and \ref empty are not representable as booleans.
   */
  std::optional<bool> as_bool() const noexcept;
  /**
   * \brief Interpret the metric value as a numeric value.
   *
   * \return
   * \li If the metric value holds a boolean, the value 0 or 1 is returned.
   * \li If the metric value holds a numeric type, the returned boolean is true iff the numeric type is non zero.
   * \li Otherwise, an empty optional is returned.
   *
   * \note string, \ref histogram and \ref empty are not representable as numbers.
   * \note the \ref signed_type is only used for negative values.
   */
  std::optional<std::variant<signed_type, unsigned_type, fp_type>> as_number()
      const noexcept;
  /**
   * \brief Interpret the metric value as a numeric value or a histogram.
   *
   * \return
   * \li If the metric value holds a boolean, the value 0 or 1 is returned.
   * \li If the metric value holds a numeric type, the returned boolean is true iff the numeric type is non zero.
   * \li If the metric value holds a histogram, the histogram is returned.
   * \li Otherwise, an empty optional is returned.
   *
   * \note string and \ref empty are not representable.
   * \note the \ref signed_type is only used for negative values.
   */
  std::optional<std::variant<signed_type, unsigned_type, fp_type, histogram>>
      as_number_or_histogram() const noexcept;
  /**
   * \brief Interpret the metric value as a string value.
   *
   * \return
   * \li If the metric value holds a boolean, the text \c "false" or \c "true" is returned.
   * \li If the metric value holds a numeric value, the \c to_string() of that value is returned.
   * \li If the metric value holds a string value, that value is returned.
   * \li Otherwise, an empty optional is returned.
   *
   * \note \ref histogram and \ref empty are not representable.
   */
  std::optional<std::string> as_string() const;
  ///@}

  /**
   * \brief Weak ordering support.
   *
   * Use this function to compare metric values in a stable ordering.
   *
   * \note The ordering is not lexicographic, nor is it based on the value of the metric value.
   */
  static bool before(const metric_value&, const metric_value&) noexcept;

 private:
  internal_types value_;
};

///\brief Logical \em not operation.
///\ingroup intf
///\relates metric_value
///\code !x \endcode
monsoon_intf_export_
metric_value operator!(const metric_value& x) noexcept;
///\brief Logical \em and operation.
///\ingroup intf
///\relates metric_value
///\code x && y \endcode
monsoon_intf_export_
metric_value operator&&(const metric_value& x, const metric_value& y) noexcept;
///\brief Logical \em or operation.
///\ingroup intf
///\relates metric_value
///\code x || y \endcode
monsoon_intf_export_
metric_value operator||(const metric_value& x, const metric_value& y) noexcept;

///\brief Negate operation.
///\ingroup intf
///\relates metric_value
///\code -x \endcode
monsoon_intf_export_
metric_value operator-(const metric_value& x) noexcept;
///\brief Addition operation.
///\ingroup intf
///\relates metric_value
///\code x + y \endcode
monsoon_intf_export_
metric_value operator+(const metric_value& x, const metric_value& y) noexcept;
///\brief Subtract operation.
///\ingroup intf
///\relates metric_value
///\code x - y \endcode
monsoon_intf_export_
metric_value operator-(const metric_value& x, const metric_value& y) noexcept;
///\brief Multiply operation.
///\ingroup intf
///\relates metric_value
///\code x * y \endcode
monsoon_intf_export_
metric_value operator*(const metric_value& x, const metric_value& y) noexcept;
///\brief Divide operation.
///\ingroup intf
///\relates metric_value
///\code x / y \endcode
monsoon_intf_export_
metric_value operator/(const metric_value& x, const metric_value& y) noexcept;
///\brief Modulo operation.
///\ingroup intf
///\relates metric_value
///\code x % y \endcode
monsoon_intf_export_
metric_value operator%(const metric_value& x, const metric_value& y) noexcept;
///\brief Shift operation.
///\ingroup intf
///\relates metric_value
///\code x << y \endcode
monsoon_intf_export_
metric_value operator<<(const metric_value& x, const metric_value& y) noexcept;
///\brief Shift operation.
///\ingroup intf
///\relates metric_value
///\code x >> y \endcode
monsoon_intf_export_
metric_value operator>>(const metric_value& x, const metric_value& y) noexcept;

/**
 * \brief Emit metric value to output stream.
 * \ingroup intf_io
 *
 * \param out The stream to which to write.
 * \param v The metric value to write to the stream.
 * \return \p out
 */
monsoon_intf_export_
std::ostream& operator<<(std::ostream& out, const metric_value& v);

///\brief Compare metric value.
///\ingroup intf
///\relates metric_value
///\code x == y \endcode
monsoon_intf_export_
metric_value equal(const metric_value& x, const metric_value& y) noexcept;
///\brief Compare metric value.
///\ingroup intf
///\relates metric_value
///\code x != y \endcode
monsoon_intf_export_
metric_value unequal(const metric_value& x, const metric_value& y) noexcept;
///\brief Compare metric value.
///\ingroup intf
///\relates metric_value
///\code x < y \endcode
monsoon_intf_export_
metric_value less(const metric_value& x, const metric_value& y) noexcept;
///\brief Compare metric value.
///\ingroup intf
///\relates metric_value
///\code x > y \endcode
monsoon_intf_export_
metric_value greater(const metric_value& x, const metric_value& y) noexcept;
///\brief Compare metric value.
///\ingroup intf
///\relates metric_value
///\code x <= y \endcode
monsoon_intf_export_
metric_value less_equal(const metric_value& x, const metric_value& y) noexcept;
///\brief Compare metric value.
///\ingroup intf
///\relates metric_value
///\code x >= y \endcode
monsoon_intf_export_
metric_value greater_equal(const metric_value& x, const metric_value& y) noexcept;

/**
 * \return Textual representation of a metric value.
 * \ingroup intf_io
 */
monsoon_intf_export_
std::string to_string(const monsoon::metric_value&);


} /* namespace monsoon */


namespace std {


///\brief STL Hash support.
///\ingroup intf_stl
template<>
struct hash<monsoon::metric_value::empty> {
  ///\brief Hashed type.
  using argument_type = const monsoon::metric_value::empty&;
  ///\brief Hash outcome.
  using result_type = size_t;

  ///\brief Compute hash code for empty metric value.
  size_t operator()(argument_type) const noexcept { return 0; }
};

///\brief STL Hash support.
///\ingroup intf_stl
template<>
struct hash<monsoon::metric_value> {
  ///\brief Hashed type.
  using argument_type = const monsoon::metric_value&;
  ///\brief Hash outcome.
  using result_type = size_t;

  ///\brief Compute hash code for metric value.
  monsoon_intf_export_
  size_t operator()(const monsoon::metric_value&) const noexcept;
};


} /* namespace std */

#include "metric_value-inl.h"

#endif /* MONSOON_METRIC_VALUE_H */
