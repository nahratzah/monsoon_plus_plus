#include <monsoon/time_series.h>

namespace monsoon {


bool time_series::operator==(const time_series& other) const noexcept {
  return tp_ == other.tp_
      && tsvs_ == other.tsvs_;
}


} /* namespace monsoon */
