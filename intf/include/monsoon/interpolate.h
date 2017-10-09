#ifndef MONSOON_INTERPOLATE_H
#define MONSOON_INTERPOLATE_H

#include <monsoon/intf_export_.h>
#include <monsoon/metric_value.h>
#include <monsoon/time_point.h>
#include <optional>

namespace monsoon {


auto monsoon_intf_export_ interpolate(time_point,
    const std::tuple<time_point, metric_value>&,
    const std::tuple<time_point, metric_value>&)
-> std::optional<metric_value>;


} /* namespace monsoon */

#endif /* MONSOON_INTERPOLATE_H */
