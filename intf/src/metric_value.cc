#include <monsoon/metric_value.h>

namespace monsoon {


inline auto metric_value::operator==(const metric_value& other) const noexcept
->  bool {
  return value_ == other.value_;
}


} /* namespace monsoon */


namespace std {


auto std::hash<monsoon::metric_value>::operator()(
    const monsoon::metric_value& v) const noexcept
->  size_t {
  if (!v.get().is_present()) return 0u;

  return monsoon::map_onto<size_t>(*v.get(),
                                   std::hash<bool>(),
                                   std::hash<long>(),
                                   std::hash<unsigned long>(),
                                   std::hash<double>(),
                                   std::hash<std::string>());
}


} /* namespace std */
