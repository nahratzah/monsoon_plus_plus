#ifndef MONSOON_HISTOGRAM_INL_H
#define MONSOON_HISTOGRAM_INL_H

#include <stdexcept>

namespace monsoon {


constexpr histogram::range::range() noexcept
: low_(0),
  high_(0)
{}

constexpr histogram::range::range(std::double_t low, std::double_t high)
: low_(low),
  high_(high)
{
  if (std::isnan(low) || std::isnan(high))
    throw std::invalid_argument("histogram::range: NaN");
  if (low > high)
    throw std::invalid_argument("histogram::range: low > high");
}

constexpr auto histogram::range::low() const noexcept -> std::double_t {
  return low_;
}

constexpr auto histogram::range::high() const noexcept -> std::double_t {
  return high_;
}

constexpr auto histogram::range::midpoint() const noexcept -> std::double_t {
  return (low() + high()) / 2;
}

constexpr auto histogram::range::width() const noexcept -> std::double_t {
  return high() - low();
}

constexpr auto histogram::range::operator==(const range& r) const noexcept
->  bool {
  return low() == r.low() && high() == r.high();
}

constexpr auto histogram::range::operator!=(const range& r) const noexcept
->  bool {
  return !(*this == r);
}

constexpr auto histogram::range::operator<(const range& r) const noexcept
->  bool {
  return ( low() != r.low()
         ? low() < r.low()
         : ( high() != r.high()
           ? high() < r.high()
           : false));
}

constexpr auto histogram::range::operator>(const range& r) const noexcept
->  bool {
  return r < *this;
}

constexpr auto histogram::range::operator<=(const range& r) const noexcept
->  bool {
  return !(r < *this);
}

constexpr auto histogram::range::operator>=(const range& r) const noexcept
->  bool {
  return !(*this < r);
}


inline histogram::histogram(histogram&& h) noexcept
: elems_(std::move(h.elems_))
{}

inline histogram& histogram::operator=(histogram&& h) noexcept {
  elems_ = std::move(h.elems_);
  return *this;
}

template<typename Iter>
histogram::histogram(Iter b, Iter e) {
  while (b != e)
    add_immed_(*b++);
  fixup_immed_unsorted_();
}

inline histogram::histogram(
    std::initializer_list<std::pair<range, std::double_t>> il)
: histogram(il.begin(), il.end())
{}

inline auto histogram::data() const noexcept -> const elems_vector& {
  return elems_;
}

inline auto histogram::min() const noexcept -> std::optional<std::double_t> {
  if (empty()) return {};
  return std::get<0>(elems_.front()).low();
}

inline auto histogram::max() const noexcept -> std::optional<std::double_t> {
  if (empty()) return {};
  return std::get<0>(elems_.back()).high();
}

inline auto histogram::avg() const noexcept -> std::optional<std::double_t> {
  if (empty()) return {};
  return sum() / count();
}

inline auto histogram::sum() const noexcept -> std::double_t {
  std::double_t result = 0;
  for (const auto& e : elems_)
    result += std::get<0>(e).midpoint() * std::get<1>(e);
  return result;
}

inline auto histogram::count() const noexcept -> std::double_t {
  if (empty()) return 0;
  return std::get<2>(elems_.back());
}

inline auto histogram::empty() const noexcept -> bool {
  return elems_.empty();
}

template<typename... T>
auto histogram::add(const std::tuple<range, std::double_t, T...>& t) -> histogram& {
  return add(std::get<0>(t), std::get<1>(t));
}

inline auto histogram::add(std::pair<range, std::double_t> p) -> histogram& {
  return add(std::get<0>(p), std::get<1>(p));
}

inline auto histogram::operator==(const histogram& h) const noexcept -> bool {
  return elems_ == h.elems_;
}

inline auto histogram::operator!=(const histogram& h) const noexcept -> bool {
  return !(*this == h);
}

template<typename... T>
auto histogram::add_immed_(const std::tuple<range, std::double_t, T...>& t)
->  void {
  add_immed_(std::get<0>(t), std::get<1>(t));
}

inline auto histogram::add_immed_(std::pair<range, std::double_t> p) -> void {
  add_immed_(std::get<0>(p), std::get<1>(p));
}


} /* namespace monsoon */


namespace std {


inline auto hash<monsoon::histogram::range>::operator()(
    const monsoon::histogram::range& r) const noexcept -> size_t {
  return 23 * std::hash<double_t>()(r.low()) ^ std::hash<double_t>()(r.high());
}


} /* namespace std */

#endif /* MONSOON_HISTOGRAM_INL_H */
