#include <monsoon/time_series_value.h>
#include <monsoon/hash_support.h>

namespace monsoon {


auto time_series_value::operator[](const metric_name& m) const noexcept
->  std::optional<metric_value> {
  auto pos = metrics_.find(m);
  if (pos == metrics_.end()) return {};
  return pos->second;
}

auto time_series_value::operator==(const time_series_value& other)
    const noexcept
->  bool {
  return name_ == other.name_ &&
         metrics_ == other.metrics_;
}


} /* namespace monsoon */


namespace std {


auto std::hash<monsoon::time_series_value>::operator()(
    const monsoon::time_series_value& v)
    const noexcept
->  size_t {
  return std::hash<monsoon::group_name>()(v.get_name());
}


} /* namespace std */
