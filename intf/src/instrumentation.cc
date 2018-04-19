#include <monsoon/instrumentation.h>

namespace monsoon {


instrumentation::group&& monsoon_instrumentation = instrumentation::make_group("monsoon");

instrumentation::group&& cache_instrumentation = instrumentation::make_group("cache", monsoon_instrumentation);


} /* namespace monsoon */
