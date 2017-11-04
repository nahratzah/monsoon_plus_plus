#ifndef MONSOON_TIME_RANGE_INL_H
#define MONSOON_TIME_RANGE_INL_H

#include <utility>

namespace monsoon {


inline auto time_range::begin() const noexcept
-> std::optional<time_point> {
  return begin_;
}

inline auto time_range::end() const noexcept
-> std::optional<time_point> {
  return end_;
}

inline auto time_range::interval() const noexcept
-> std::optional<time_point::duration> {
  return interval_;
}

inline auto time_range::begin(time_point tp) noexcept
-> std::optional<time_point> {
  return std::exchange(begin_, std::make_optional(tp));
}

inline auto time_range::end(time_point tp) noexcept
-> std::optional<time_point> {
  return std::exchange(end_, std::make_optional(tp));
}

inline auto time_range::interval(time_point::duration d) noexcept
-> std::optional<time_point::duration> {
  return std::exchange(interval_, std::make_optional(d));
}

inline auto time_range::reset_begin() noexcept
-> std::optional<time_point> {
  return std::exchange(begin_, std::optional<time_point>());
}

inline auto time_range::reset_end() noexcept
-> std::optional<time_point> {
  return std::exchange(end_, std::optional<time_point>());
}

inline auto time_range::reset_interval() noexcept
-> std::optional<time_point::duration> {
  return std::exchange(interval_, std::optional<time_point::duration>());
}


inline bool operator==(const time_range& x, const time_range& y) noexcept {
  return x.begin() == y.begin()
      && x.end() == y.end()
      && x.interval() == y.interval();
}

inline bool operator!=(const time_range& x, const time_range& y) noexcept {
  return !(x == y);
}


} /* namespace monsoon */

#endif /* MONSOON_TIME_RANGE_INL_H */
