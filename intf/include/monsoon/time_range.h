#ifndef MONSOON_TIME_RANGE_H
#define MONSOON_TIME_RANGE_H

///\file
///\ingroup intf

#include <monsoon/intf_export_.h>
#include <monsoon/time_point.h>
#include <optional>
#include <iosfwd>

namespace monsoon {


/**
 * \brief A time range.
 * \ingroup intf
 *
 * \details A time range consists of:
 * \li an optional begin
 * \li an optional end
 * \li an optional interval
 *
 * The begin time point must be at or before the end time point.
 */
class monsoon_intf_export_ time_range {
 public:
  /**
   * \brief The begin of the time range.
   *
   * If the begin is absent, the time range represents a time range without a
   * begin. In this case, any request for data over time is to assume begin as
   * the minimum sensible value for the request.
   */
  std::optional<time_point> begin() const noexcept;
  /**
   * \brief The end of the time range.
   *
   * If the end is absent, the time range never ends. In this case, any request
   * for data over time is to assume end as the maximum sensible value for the
   * request.
   */
  std::optional<time_point> end() const noexcept;
  /**
   * \brief The interval of the time range.
   *
   * If present, a request over time is to emit values at
   * \f${\tt begin()} + n * {\tt interval()}\f$
   * where
   * \f$n \in {\mathbb N}_{0}\f$
   * and
   * \f${\tt begin()} + n * {\tt interval()} \leq {\tt end()}\f$.
   *
   * If no
   * \f$n \in {\mathbb N}_{0}\f$
   * exists, such that
   * \f${\tt begin()} + n * {\tt interval()} = {\tt end()}\f$,
   * an additional emit for \f${\tt end()}\f$ shall be generated.
   *
   * \todo Figure out how to omit this math formula from anything not handling images (aka man pages).
   */
  std::optional<time_point::duration> interval() const noexcept;

  /**
   * \brief Set the begin time point.
   * \param tp value to be assigned to \ref begin().
   * \return the previous value of \ref begin().
   */
  std::optional<time_point> begin(time_point tp) noexcept;
  /**
   * \brief Set the end time point.
   * \param tp value to be assigned to \ref end().
   * \return the previous value of \ref end().
   */
  std::optional<time_point> end(time_point tp) noexcept;
  /**
   * \brief Set the interval duration.
   * \param d value to be assigned to \ref interval().
   * \return the previous value of \ref interval().
   */
  std::optional<time_point::duration> interval(time_point::duration d) noexcept;

  /**
   * \brief Clear the begin time point.
   * \return the previous value of \ref begin().
   */
  std::optional<time_point> reset_begin() noexcept;
  /**
   * \brief Clear the end time point.
   * \return the previous value of \ref end().
   */
  std::optional<time_point> reset_end() noexcept;
  /**
   * \brief Clear the interval duration.
   * \return the previous value of \ref interval().
   */
  std::optional<time_point::duration> reset_interval() noexcept;

 private:
  std::optional<time_point> begin_, end_;
  std::optional<time_point::duration> interval_;
};

/**
 * \brief Compare time range for equality.
 * \relates time_range
 * \ingroup intf
 */
bool operator==(const time_range& x, const time_range& y) noexcept;
/**
 * \brief Compare time range for inequality.
 * \relates time_range
 * \ingroup intf
 */
bool operator!=(const time_range& x, const time_range& y) noexcept;

/**
 * \brief String representation of time_range.
 * \ingroup intf_io
 * \relates time_range
 * \ingroup intf
 */
monsoon_intf_export_
std::string to_string(const time_range& tr);

/**
 * \brief Print textual representation of time_range.
 * \ingroup intf_io
 * \relates time_range
 * \ingroup intf
 */
monsoon_intf_export_
std::ostream& operator<<(std::ostream& out, const time_range& tr);


} /* namespace monsoon */

#include "time_range-inl.h"

#endif /* MONSOON_TIME_RANGE_H */
