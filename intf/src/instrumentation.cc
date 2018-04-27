#include <monsoon/instrumentation.h>

namespace monsoon {


instrumentation::group& monsoon_instrumentation() {
  static auto impl = instrumentation::make_group("monsoon");
  return impl;
}

instrumentation::group& cache_instrumentation() {
  static auto impl = instrumentation::make_group("cache", monsoon_instrumentation());
  return impl;
}


} /* namespace monsoon */
