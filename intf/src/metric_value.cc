#include <monsoon/metric_value.h>

namespace monsoon {


inline auto metric_value::operator==(const metric_value& other) const noexcept
->  bool {
  return value_ == other.value_;
}


} /* namespace monsoon */
