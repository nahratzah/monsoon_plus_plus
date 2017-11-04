#ifndef MONSOON_TIME_POINT_INL_H
#define MONSOON_TIME_POINT_INL_H

namespace monsoon {


constexpr time_point::time_point(std::int64_t millis_since_epoch) noexcept
: millis_(millis_since_epoch)
{}

constexpr std::int64_t time_point::millis_since_posix_epoch() const noexcept {
  return millis_;
}

constexpr bool time_point::operator==(const time_point& y) const noexcept {
  return millis_ == y.millis_;
}

constexpr bool time_point::operator!=(const time_point& y) const noexcept {
  return !(*this == y);
}

constexpr bool time_point::operator<(const time_point& y) const noexcept {
  return millis_ < y.millis_;
}

constexpr bool time_point::operator>(const time_point& y) const noexcept {
  return millis_ > y.millis_;
}

constexpr bool time_point::operator<=(const time_point& y) const noexcept {
  return millis_ <= y.millis_;
}

constexpr bool time_point::operator>=(const time_point& y) const noexcept {
  return millis_ >= y.millis_;
}

constexpr time_point& time_point::operator+=(const duration& d) noexcept {
  millis_ += d.millis();
  return *this;
}

constexpr time_point& time_point::operator-=(const duration& d) noexcept {
  millis_ -= d.millis();
  return *this;
}


constexpr time_point::duration::duration(std::int64_t millis) noexcept
: millis_(millis)
{}

constexpr time_point::duration::duration(time_point b, time_point e) noexcept
: duration(e.millis_since_posix_epoch() - b.millis_since_posix_epoch())
{}

constexpr std::int64_t time_point::duration::millis() const noexcept {
  return millis_;
}

constexpr bool time_point::duration::operator==(const time_point::duration& y)
    const noexcept {
  return millis_ == y.millis_;
}

constexpr bool time_point::duration::operator!=(const time_point::duration& y)
    const noexcept {
  return !(*this == y);
}

constexpr bool time_point::duration::operator<(const time_point::duration& y)
    const noexcept {
  return millis_ < y.millis_;
}

constexpr bool time_point::duration::operator>(const time_point::duration& y)
    const noexcept {
  return millis_ > y.millis_;
}

constexpr bool time_point::duration::operator<=(const time_point::duration& y)
    const noexcept {
  return millis_ <= y.millis_;
}

constexpr bool time_point::duration::operator>=(const time_point::duration& y)
    const noexcept {
  return millis_ >= y.millis_;
}

constexpr auto time_point::duration::operator+=(duration y) noexcept
-> duration& {
  millis_ += y.millis_;
  return *this;
}

constexpr auto time_point::duration::operator-=(duration y) noexcept
-> duration& {
  millis_ -= y.millis_;
  return *this;
}

constexpr auto operator+(time_point::duration x, time_point::duration y)
-> time_point::duration {
  return x += y;
}

constexpr auto operator-(time_point::duration x, time_point::duration y)
-> time_point::duration {
  return x -= y;
}

constexpr auto operator+(time_point x, time_point::duration y) noexcept
-> time_point {
  return x += y;
}

constexpr auto operator-(time_point x, time_point::duration y) noexcept
-> time_point {
  return x -= y;
}

constexpr auto operator-(time_point x, time_point y) noexcept
-> time_point::duration {
  return time_point::duration(y, x);
}


} /* namespace monsoon */

#endif /* MONSOON_TIME_POINT_INL_H */
