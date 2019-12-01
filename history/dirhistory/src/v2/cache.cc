#include "cache.h"
#include <monsoon/cache/impl.h>

namespace monsoon::history::v2 {


auto get_dynamics_cache_() -> cache_type& {
  static cache_type impl = cache_type::builder()
      .access_expire(std::chrono::minutes(15))
      .async(true)
      .max_memory(256 * 1024 * 1024)
      .stats("cache")
      .build(dynamics_cache_create());
  return impl;
}


} /* namespace monsoon::history::v2 */
