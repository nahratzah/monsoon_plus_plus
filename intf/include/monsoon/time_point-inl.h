#ifndef MONSOON_TIME_POINT_INL_H
#define MONSOON_TIME_POINT_INL_H

namespace monsoon {


inline time_point::time_point(std::int64_t millis_since_epoch) noexcept
: millis_(millis_since_epoch)
{}

inline std::int64_t time_point::millis_since_posix_epoch() const noexcept {
  return millis_;
}

inline bool time_point::operator==(const time_point& y) const noexcept {
  return millis_ == y.millis_;
}

inline bool time_point::operator<(const time_point& y) const noexcept {
  return millis_ < y.millis_;
}


inline time_point::duration::duration(std::int64_t millis) noexcept
: millis_(millis)
{}

inline time_point::duration::duration(time_point b, time_point e) noexcept
: duration(e.millis_since_posix_epoch() - b.millis_since_posix_epoch())
{}

inline std::int64_t time_point::duration::millis() const noexcept {
  return millis_;
}

inline auto time_point::duration::operator+=(duration y) noexcept -> duration& {
  millis_ += y.millis_;
  return *this;
}

inline auto time_point::duration::operator-=(duration y) noexcept -> duration& {
  millis_ -= y.millis_;
  return *this;
}

inline auto operator+(time_point::duration x, time_point::duration y)
-> time_point::duration {
  return x += y;
}

inline auto operator-(time_point::duration x, time_point::duration y)
-> time_point::duration {
  return x -= y;
}


} /* namespace monsoon */

#endif /* MONSOON_TIME_POINT_INL_H */
