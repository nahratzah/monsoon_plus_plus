#ifndef MONSOON_INTERPOLATE_H
#define MONSOON_INTERPOLATE_H

///\file
///\ingroup intf

#include <monsoon/intf_export_.h>
#include <monsoon/metric_value.h>
#include <monsoon/time_point.h>
#include <optional>

namespace monsoon {


/**
 * \brief Interpolate a value at at time point, using linear interpolation.
 * \ingroup intf
 *
 * \param tp The time point for which to interpolate the metric value.
 * \param x A (time point, value) tuple before \p tp.
 * \param y A (time point, value) tuple after \p tp.
 * \return the interpolated metric value at \p tp, if it is computable.
 *   If the interpolation fails, an empty optional is returned.
 * \throw std::invalid_argument
 *   If \p x and \p y have the same time stamp
 * \throw std::invalid_argument
 *   If \p tp is outside the range between \p x and \p y.
 * \throw std::invalid_argument
 *   If \p tp is equal to either \p x or \p y.
 * \relates metric_value
 */
auto monsoon_intf_export_ interpolate(time_point tp,
    std::tuple<time_point, const metric_value&> x,
    std::tuple<time_point, const metric_value&> y)
-> std::optional<metric_value>;


} /* namespace monsoon */

#endif /* MONSOON_INTERPOLATE_H */
