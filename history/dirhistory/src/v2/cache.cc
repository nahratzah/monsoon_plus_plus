#include "cache.h"
#include <monsoon/cache/impl.h>
#include <monsoon/history/instrumentation.h>
#include <instrumentation/group.h>

namespace monsoon::history::v2 {


instrumentation::group& cache_grp() {
  static auto impl = instrumentation::make_group("tsdata", history_instrumentation());
  return impl;
}

auto get_dynamics_cache_() -> cache_type& {
  static cache_type impl = cache_type::builder()
      .access_expire(std::chrono::minutes(15))
      .async(true)
      .max_memory(256 * 1024 * 1024)
      .stats("cache", cache_grp())
      .build(dynamics_cache_create());
  return impl;
}


} /* namespace monsoon::history::v2 */
