#ifndef MONSOON_TIME_POINT_H
#define MONSOON_TIME_POINT_H

///\file
///\ingroup intf

#include <monsoon/intf_export_.h>
#include <cstdint>
#include <iosfwd>

namespace monsoon {


/**
 * \brief A point in time, at millisecond resolution.
 * \ingroup intf
 *
 * A time point describes a point in time in millisecond resolution.
 *
 * \note Time point uses the unix/posix epoch.
 */
class time_point {
 public:
  class duration;

  time_point() noexcept = default;
  /**
   * \brief Construct a time point at the given offset, in msec, from the unix/posix epoch.
   * \param millis Number of milliseconds since Jan 1, 1970 00:00:00Z.
   */
  explicit constexpr time_point(std::int64_t millis) noexcept;
  /**
   * \brief Construct a time point by parsing the given string.
   * \param s A string representation of the form <tt>YYYYMMdd<b>T</b>HH:mm:ss.sss<b>Z</b></tt>.
   */
  monsoon_intf_export_ explicit time_point(const std::string& s);

  /**
   * \return the number of milliseconds since posix epoch.
   */
  std::int64_t constexpr millis_since_posix_epoch() const noexcept;
  /**
   * \brief Create a time point representing the current time.
   * \return The current time.
   * \note This depends on the wall clock of the system being correct.
   */
  monsoon_intf_export_ static time_point now();

  ///@{
  ///\brief Compare time points.
  constexpr bool operator==(const time_point&) const noexcept;
  ///\brief Compare time points.
  constexpr bool operator!=(const time_point&) const noexcept;
  ///\brief Compare time points.
  constexpr bool operator<(const time_point&) const noexcept;
  ///\brief Compare time points.
  constexpr bool operator>(const time_point&) const noexcept;
  ///\brief Compare time points.
  constexpr bool operator<=(const time_point&) const noexcept;
  ///\brief Compare time points.
  constexpr bool operator>=(const time_point&) const noexcept;
  ///@}

  ///@{
  ///\brief Add a duration to this time point.
  constexpr time_point& operator+=(const duration&) noexcept;
  ///\brief Add a duration to this time point.
  constexpr time_point& operator-=(const duration&) noexcept;
  ///@}

  ///\brief Extract the year, according to the gregorian calendar.
  monsoon_intf_export_ int year() const noexcept;
  ///\brief Extract the month, according to the gregorian calendar.
  monsoon_intf_export_ int month() const noexcept;
  ///\brief Extract the day of the month, according to the gregorian calendar.
  monsoon_intf_export_ int day_of_month() const noexcept;
  ///\brief Extract the hour of the day, according to the gregorian calendar.
  monsoon_intf_export_ int hour() const noexcept;
  ///\brief Extract the minute of the hour, according to the gregorian calendar.
  monsoon_intf_export_ int minute() const noexcept;
  ///\brief Extract the second of the minute, according to the gregorian calendar.
  ///\note Seconds are rounded down, the returned value omits milliseconds.
  monsoon_intf_export_ int second() const noexcept;

 private:
  std::int64_t millis_;
};

/**
 * \brief Represents a time duration with millisecond resolution.
 * \ingroup intf
 */
class time_point::duration {
 public:
  duration() noexcept = default;
  /**
   * \brief Construct a duration with the given number of milliseconds.
   * \param millis The number of milliseconds in this duration.
   */
  explicit constexpr duration(std::int64_t millis) noexcept;
  /**
   * \brief Construct a duration based on the given time points.
   * \param x,y \parblock
   *   The time points between which to compute the duration.
   *   \code{.cc} y - x \endcode
   * \endparblock
   */
  constexpr duration(time_point x, time_point y) noexcept;

  /**
   * \brief The time duration in this duration.
   * \return The number of milliseconds in this duration.
   */
  constexpr std::int64_t millis() const noexcept;

  ///@{
  ///\brief Add a duration to this duration.
  constexpr duration& operator+=(duration) noexcept;
  ///\brief Subtract a duration to this duration.
  constexpr duration& operator-=(duration) noexcept;
  ///@}

 private:
  std::int64_t millis_;
};

/**
 * \brief Add two duration together.
 * \ingroup intf
 * \relates time_point::duration
 * \param x,y The durations to sum.
 * \return The sum of the durations.
 */
constexpr auto operator+(time_point::duration x, time_point::duration y)
-> time_point::duration;

/**
 * \brief Subtract two duration together.
 * \ingroup intf
 * \relates time_point::duration
 * \param x The left hand side of the subtraction.
 * \param y The durations to subtract from \p x.
 * \return The sum of the durations.
 */
constexpr auto operator-(time_point::duration x, time_point::duration y)
-> time_point::duration;

/**
 * \brief Compute a time point by adding a duration.
 * \ingroup intf
 * \relates time_point
 * \relates time_point::duration
 * \param tp A time point.
 * \param d A duration to add.
 * \return a time point that is \p d after \p tp.
 */
constexpr auto operator+(time_point tp, time_point::duration d) noexcept
-> time_point;

/**
 * \brief Compute a time point by subtracting a duration.
 * \ingroup intf
 * \relates time_point
 * \relates time_point::duration
 * \param tp A time point.
 * \param d A duration to subtract.
 * \return a time point that is \p d before \p tp.
 */
constexpr auto operator-(time_point tp, time_point::duration d) noexcept
-> time_point;

/**
 * \brief Compute the duration between two time points.
 * \ingroup intf
 * \relates time_point
 * \relates time_point::duration
 * \param x,y Time points between which to compute the duration.
 * \return \code{.cc} x - y \endcode
 */
constexpr auto operator-(time_point x, time_point y) noexcept
-> time_point::duration;

/**
 * \brief Yield a string representation of the time point.
 * \ingroup intf_io
 * \relates time_point
 * \return The string representation of the time point.
 */
monsoon_intf_export_
std::string to_string(time_point);

/**
 * \brief Write the text representation of the time point to a stream.
 * \ingroup intf_io
 * \relates time_point
 * \param out The output stream to which to write.
 * \param tp The time point for which to write a textual representation.
 * \return \p out
 */
monsoon_intf_export_
auto operator<<(std::ostream& out, time_point tp) -> std::ostream&;


} /* namespace monsoon */

#include "time_point-inl.h"

#endif /* MONSOON_TIME_POINT_H */
