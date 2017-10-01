#ifndef MONSOON_TIME_POINT_H
#define MONSOON_TIME_POINT_H

#include <monsoon/intf_export_.h>
#include <cstdint>
#include <iosfwd>

namespace monsoon {


class time_point {
 public:
  class duration;

  time_point() noexcept = default;
  time_point(const time_point&) noexcept = default;
  time_point& operator=(const time_point&) noexcept = default;
  explicit time_point(std::int64_t) noexcept;
  monsoon_intf_export_ explicit time_point(const std::string&);

  std::int64_t millis_since_posix_epoch() const noexcept;
  monsoon_intf_export_ static time_point now();

  bool operator==(const time_point&) const noexcept;
  bool operator!=(const time_point&) const noexcept;
  bool operator<(const time_point&) const noexcept;

  time_point& operator+=(const duration&) noexcept;
  time_point& operator-=(const duration&) noexcept;

 private:
  std::int64_t millis_;
};

class time_point::duration {
 public:
  duration() noexcept = default;
  duration(const duration&) noexcept = default;
  duration& operator=(const duration&) noexcept = default;
  explicit duration(std::int64_t) noexcept;
  duration(time_point, time_point) noexcept;

  std::int64_t millis() const noexcept;

  duration& operator+=(duration) noexcept;
  duration& operator-=(duration) noexcept;

 private:
  std::int64_t millis_;
};

auto operator+(time_point::duration, time_point::duration)
-> time_point::duration;

auto operator-(time_point::duration, time_point::duration)
-> time_point::duration;

auto operator+(time_point, time_point::duration) noexcept
-> time_point;

auto operator-(time_point, time_point::duration) noexcept
-> time_point;

monsoon_intf_export_
std::string to_string(time_point);

monsoon_intf_export_
auto operator<<(std::ostream&, time_point) -> std::ostream&;


} /* namespace monsoon */

#include "time_point-inl.h"

#endif /* MONSOON_TIME_POINT_H */
