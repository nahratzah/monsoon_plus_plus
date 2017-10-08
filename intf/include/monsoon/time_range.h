#ifndef MONSOON_TIME_RANGE_H
#define MONSOON_TIME_RANGE_H

#include <monsoon/intf_export_.h>
#include <monsoon/time_point.h>
#include <optional>

namespace monsoon {


class monsoon_intf_export_ time_range {
 public:
  time_range() noexcept = default;
  time_range(const time_range&) noexcept = default;
  time_range& operator=(const time_range&) noexcept = default;
  ~time_range() noexcept = default;

  std::optional<time_point> begin() const noexcept;
  std::optional<time_point> end() const noexcept;
  std::optional<time_point::duration> interval() const noexcept;

  std::optional<time_point> begin(time_point) noexcept;
  std::optional<time_point> end(time_point) noexcept;
  std::optional<time_point::duration> interval(time_point::duration) noexcept;

  std::optional<time_point> reset_begin() noexcept;
  std::optional<time_point> reset_end() noexcept;
  std::optional<time_point::duration> reset_interval() noexcept;

 private:
  std::optional<time_point> begin_, end_;
  std::optional<time_point::duration> interval_;
};


} /* namespace monsoon */

#include "time_range-inl.h"

#endif /* MONSOON_TIME_RANGE_H */
