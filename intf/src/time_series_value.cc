#include <monsoon/time_series_value.h>
#include <monsoon/hash_support.h>

namespace monsoon {


auto time_series_value::operator[](const metric_name& m) const noexcept
->  optional<const metric_value&> {
  auto pos = metrics_.find(m);
  return ( pos == metrics_.end()
         ? optional<const metric_value&>()
         : pos->second);
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


auto std::hash<monsoon::time_series_value>::operator()(
    const monsoon::time_series_value& v)
    const noexcept
->  size_t {
  const auto tp_val = v.get_time().time_since_epoch().count();
  return std::hash<remove_const_t<decltype(tp_val)>>()(tp_val) ^
         std::hash<monsoon::group_name>()(v.get_name()) ^
         monsoon::map_to_hash(v.get_metrics());
}


} /* namespace std */
