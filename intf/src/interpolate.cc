#include <monsoon/interpolate.h>

namespace monsoon {


auto interpolate(time_point at,
    const std::tuple<time_point, metric_value>& x,
    const std::tuple<time_point, metric_value>& y)
-> std::optional<metric_value> {
  if (std::get<0>(x) == std::get<0>(y))
    throw std::invalid_argument("interpolate same timestamp");
  if (std::get<0>(x) > at)
    throw std::invalid_argument("interpolate at");
  if (std::get<0>(y) < at)
    throw std::invalid_argument("interpolate at");

  if (std::get<0>(x) == at)
    return std::get<1>(x);
  if (std::get<0>(y) == at)
    return std::get<1>(y);

  const auto xdiff = (at - std::get<0>(x)).millis();
  const auto ydiff = (std::get<0>(y) - at).millis();
  const long double diff =
      static_cast<long double>(xdiff) + static_cast<long double>(ydiff);

  const auto xweight = ydiff / diff;
  const auto yweight = xdiff / diff;

  return visit(
      [xweight, yweight](const auto& xval, const auto& yval) -> std::optional<metric_value> {
        using x_type = std::decay_t<decltype(xval)>;
        using y_type = std::decay_t<decltype(yval)>;

        if constexpr(std::is_same_v<metric_value::empty, x_type>
            || std::is_same_v<metric_value::empty, y_type>)
          return metric_value();
        else if constexpr(std::is_same_v<bool, x_type>
            || std::is_same_v<std::string, x_type>
            || std::is_same_v<bool, y_type>
            || std::is_same_v<std::string, y_type>)
          return metric_value(xval);
        else if constexpr(
            (std::is_same_v<metric_value::signed_type, x_type>
             || std::is_same_v<metric_value::unsigned_type, x_type>
             || std::is_same_v<metric_value::fp_type, x_type>)
            && (std::is_same_v<metric_value::signed_type, y_type>
             || std::is_same_v<metric_value::unsigned_type, y_type>
             || std::is_same_v<metric_value::fp_type, y_type>))
          return metric_value(static_cast<double>(xweight * xval + yweight * yval));
        else if constexpr(std::is_same_v<histogram, x_type>
            && std::is_same_v<histogram, y_type>)
          return metric_value(xweight * xval + yweight * yval);
        else
          return {};
      },
      std::get<1>(x).get(), std::get<1>(y).get());
}


} /* namespace monsoon */
