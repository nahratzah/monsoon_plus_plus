#ifndef MONSOON_HISTOGRAM_H
#define MONSOON_HISTOGRAM_H

#include <monsoon/optional.h>
#include <cmath>
#include <functional>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

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


class histogram {
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
  class range {
   public:
    constexpr range() noexcept = default;
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
    std::double_t low_ = 0, high_ = 0;
  };

  using elems_vector =
      std::vector<std::tuple<range, std::double_t, std::double_t>>;

  histogram() = default;
  histogram(const histogram&) = default;
  histogram(histogram&&) noexcept;
  template<typename Iter> histogram(Iter, Iter);

  std::map<range, std::double_t> map() const;
  const elems_vector& data() const noexcept;
  optional<std::double_t> min() const noexcept;
  optional<std::double_t> max() const noexcept;
  optional<std::double_t> avg() const noexcept;
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


} /* namespace monsoon */


namespace std {


template<>
struct hash<monsoon::histogram::range> {
  using argument_type = const monsoon::histogram::range&;
  using result_type = std::size_t;

  size_t operator()(const monsoon::histogram::range&) const noexcept;
};

template<>
struct hash<monsoon::histogram> {
  using argument_type = const monsoon::histogram&;
  using result_type = std::size_t;

  size_t operator()(const monsoon::histogram&) const noexcept;
};


} /* namespace std */

#include "histogram-inl.h"

#endif /* MONSOON_HISTOGRAM_H */