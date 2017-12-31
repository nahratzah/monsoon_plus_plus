#ifndef MONSOON_HISTOGRAM_H
#define MONSOON_HISTOGRAM_H

#include <monsoon/intf_export_.h>
#include <optional>
#include <cmath>
#include <functional>
#include <iosfwd>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>
#include <initializer_list>

namespace monsoon {


class histogram;

histogram operator-(histogram) noexcept;
histogram operator+(histogram&&, const histogram&);
histogram operator+(const histogram&, histogram&&);
histogram operator+(histogram&&, histogram&&);
histogram operator+(const histogram&, const histogram&);
histogram operator-(histogram&&, histogram&&);
histogram operator-(const histogram&, histogram&&);
histogram operator-(histogram&&, const histogram&);
histogram operator-(const histogram&, const histogram&);
histogram operator*(histogram, std::double_t) noexcept;
histogram operator/(histogram, std::double_t);
histogram operator*(std::double_t, histogram) noexcept;
histogram& operator-=(histogram&, const histogram&);
histogram& operator+=(histogram&, const histogram&);
histogram& operator*=(histogram&, std::double_t);
histogram& operator/=(histogram&, std::double_t);


class monsoon_intf_export_ histogram {
  friend histogram operator-(histogram) noexcept;
  friend histogram operator+(histogram&&, const histogram&);
  friend histogram operator+(const histogram&, histogram&&);
  friend histogram operator+(histogram&&, histogram&&);
  friend histogram operator+(const histogram&, const histogram&);
  friend histogram operator-(histogram&&, histogram&&);
  friend histogram operator-(const histogram&, histogram&&);
  friend histogram operator-(histogram&&, const histogram&);
  friend histogram operator-(const histogram&, const histogram&);
  friend histogram operator*(histogram, std::double_t) noexcept;
  friend histogram operator/(histogram, std::double_t);
  friend histogram operator*(std::double_t, histogram) noexcept;
  friend histogram& operator-=(histogram&, const histogram&);
  friend histogram& operator+=(histogram&, const histogram&);
  friend histogram& operator*=(histogram&, std::double_t);
  friend histogram& operator/=(histogram&, std::double_t);

 public:
  class monsoon_intf_local_ range {
   public:
    constexpr range() noexcept;
    constexpr range(const range&) noexcept = default;
    constexpr range(std::double_t, std::double_t);
    range& operator=(const range&) noexcept = default;

    constexpr std::double_t low() const noexcept;
    constexpr std::double_t high() const noexcept;
    constexpr std::double_t midpoint() const noexcept;
    constexpr std::double_t width() const noexcept;

    constexpr bool operator==(const range&) const noexcept;
    constexpr bool operator!=(const range&) const noexcept;
    constexpr bool operator<(const range&) const noexcept;
    constexpr bool operator>(const range&) const noexcept;
    constexpr bool operator<=(const range&) const noexcept;
    constexpr bool operator>=(const range&) const noexcept;

   private:
    std::double_t low_, high_;
  };

  using elems_vector =
      std::vector<std::tuple<range, std::double_t, std::double_t>>;

  histogram() = default;
  histogram(const histogram&) = default;
  histogram(histogram&&) noexcept;
  histogram& operator=(const histogram&) = default;
  histogram& operator=(histogram&&) noexcept;
  template<typename Iter> histogram(Iter, Iter);
  histogram(std::initializer_list<std::pair<range, std::double_t>>);

  std::map<range, std::double_t> map() const;
  const elems_vector& data() const noexcept;
  std::optional<std::double_t> min() const noexcept;
  std::optional<std::double_t> max() const noexcept;
  std::optional<std::double_t> avg() const noexcept;
  std::double_t sum() const noexcept;
  std::double_t count() const noexcept;
  bool empty() const noexcept;

  histogram& add(range, std::double_t);
  template<typename... T> histogram& add(
      const std::tuple<range, std::double_t, T...>&);
  histogram& add(std::pair<range, std::double_t>);

  bool operator==(const histogram&) const noexcept;
  bool operator!=(const histogram&) const noexcept;
  static bool before(const histogram&, const histogram&) noexcept;

  static histogram parse(std::string_view s);

 private:
  void add_immed_(range, std::double_t);
  template<typename... T> void add_immed_(
      const std::tuple<range, std::double_t, T...>&);
  void add_immed_(std::pair<range, std::double_t>);
  void fixup_immed_unsorted_();
  void fixup_immed_();
  void fixup_immed_erase_empty_();

  elems_vector elems_;
};

monsoon_intf_export_
std::string to_string(const monsoon::histogram&);

monsoon_intf_export_
std::ostream& operator<<(std::ostream&, const histogram&);


} /* namespace monsoon */


namespace std {


///\brief STL Hash support.
///\ingroup intf_stl
///\relates histogram::range
template<>
struct hash<monsoon::histogram::range> {
  ///\brief Argument type for hash function.
  using argument_type = const monsoon::histogram::range&;
  ///\brief Result type for hash function.
  using result_type = std::size_t;

  ///\brief Compute hash code for histogram::range.
  monsoon_intf_export_
  size_t operator()(const monsoon::histogram::range&) const noexcept;
};

///\brief STL Hash support.
///\ingroup intf_stl
///\relates histogram
template<>
struct hash<monsoon::histogram> {
  ///\brief Argument type for hash function.
  using argument_type = const monsoon::histogram&;
  ///\brief Result type for hash function.
  using result_type = std::size_t;

  ///\brief Compute hash code for histogram.
  monsoon_intf_export_
  size_t operator()(const monsoon::histogram&) const noexcept;
};


} /* namespace std */

#include "histogram-inl.h"

#endif /* MONSOON_HISTOGRAM_H */
