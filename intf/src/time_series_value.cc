#include <monsoon/time_series_value.h>

namespace monsoon {


auto time_series_value::operator[](const metric_name& m) const noexcept
->  optional<const metric_value&> {
  auto pos = metrics_.find(m);
  return ( pos == metrics_.end()
         ? optional<const metric_value&>()
         : *pos);
}

auto time_series_value::operator==(const time_series_value& other)
    const noexcept
->  bool {
  return tp_ == other.tp_ &&
         name_ == other.name_ &&
         metrics_ == other.metrics_;
}


} /* namespace monsoon */


namespace std {


auto std::hash<monsoon::time_series_value>::operator(
    const monsoon::time_series_value& v)
    const noexcept
->  size_t {
  return std::hash<monsoon::time_series_value::time_point>()(v.get_time()) ^
         std::hash<monsoon::group_name>()(v.get_name()) ^
         std::hash<monsoon::time_series_value::metric_map>()(v.get_metrics());
}


} /* namespace std */
