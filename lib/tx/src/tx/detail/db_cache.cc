#include <monsoon/tx/detail/db_cache.h>
#include <monsoon/cache/builder.h>
#include <monsoon/cache/impl.h>

namespace monsoon::tx::detail {


db_cache_impl::db_cache_impl(std::string name, std::uintptr_t max_memory)
: impl_(impl_type::builder()
    .access_expire(std::chrono::minutes(15))
    .enable_async()
    .max_memory(max_memory)
    .stats(std::move(name))
    .build(create()))
{}

db_cache_impl::~db_cache_impl() noexcept = default;

auto db_cache_impl::get_if_present(
    txfile::transaction::offset_type off,
    cycle_ptr::cycle_gptr<const domain> dom) noexcept
-> cycle_ptr::cycle_gptr<cache_obj> {
  return impl_.get_if_present(key(off, std::move(dom)));
}

auto db_cache_impl::get(
    txfile::transaction::offset_type off,
    cycle_ptr::cycle_gptr<const domain> dom,
    cheap_fn_ref<cycle_ptr::cycle_gptr<cache_obj>(allocator_type, txfile::transaction::offset_type)> load)
-> cycle_ptr::cycle_gptr<cache_obj> {
  return impl_.get(search{ key(off, std::move(dom)), std::move(load) });
}

void db_cache_impl::invalidate(
    txfile::transaction::offset_type off,
    cycle_ptr::cycle_gptr<const domain> dom) noexcept {
#if 0
  impl_.invalidate(key(off, std::move(dom)));
#endif
}


db_cache_impl::domain::~domain() noexcept = default;


db_cache_impl::cache_obj::~cache_obj() noexcept = default;


} /* namespace monsoon::tx::detail */
