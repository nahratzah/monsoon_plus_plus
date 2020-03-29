#include <monsoon/tx/detail/db_cache.h>
#include <monsoon/cache/builder.h>
#include <monsoon/cache/impl.h>

namespace monsoon::tx::detail {


db_cache::db_cache(std::string name, std::uintptr_t max_memory, shared_resource_allocator<std::byte> allocator)
: impl_(impl_type::builder()
    .enable_async()
    .max_memory(max_memory)
    .stats(std::move(name))
    .template storage_override<cycle_ptr::cycle_member_ptr<cache_obj>>(
        [this](const cycle_ptr::cycle_gptr<cache_obj>& ptr) -> std::tuple<cycle_ptr::cycle_base&, const cycle_ptr::cycle_gptr<cache_obj>&> {
          return std::forward_as_tuple(*this, ptr);
        },
        [](const cycle_ptr::cycle_member_ptr<cache_obj>& ptr) -> cycle_ptr::cycle_gptr<cache_obj> {
          return ptr;
        })
    .allocator(allocator_type(allocator))
    .build(create_cb()))
{}

db_cache::~db_cache() noexcept = default;

auto db_cache::get_if_present(
    txfile::transaction::offset_type off,
    cycle_ptr::cycle_gptr<const domain> dom) noexcept
-> cycle_ptr::cycle_gptr<cache_obj> {
  return impl_.get_if_present(key(off, std::move(dom)));
}

auto db_cache::get(
    txfile::transaction::offset_type off,
    cycle_ptr::cycle_gptr<const domain> dom,
    cheap_fn_ref<cycle_ptr::cycle_gptr<cache_obj>(allocator_type, txfile::transaction::offset_type)> load)
-> cycle_ptr::cycle_gptr<cache_obj> {
  return impl_.get(search{ key(off, std::move(dom)), std::move(load) });
}

auto db_cache::create(
    txfile::transaction::offset_type off,
    cycle_ptr::cycle_gptr<const domain> dom,
    cheap_fn_ref<cycle_ptr::cycle_gptr<cache_obj>(allocator_type, txfile::transaction::offset_type)> load,
    tx_op_collection& ops)
-> cycle_ptr::cycle_gptr<cache_obj> {
  bool was_called_ = false;

  auto r = impl_.get(
      search{
        key(off, dom),
        [&was_called_, &load](allocator_type alloc, txfile::transaction::offset_type off) {
          was_called_ = true;
          return load(std::move(alloc), std::move(off));
        }
      });
  if (!was_called_) throw std::runtime_error("cache corruption: creation at given offset failed because an eleemnt is present");
  invalidate_on_rollback(std::move(off), std::move(dom), ops);
  return r;
}

void db_cache::invalidate(
    txfile::transaction::offset_type off,
    cycle_ptr::cycle_gptr<const domain> dom) noexcept {
  impl_.expire(key(off, std::move(dom)));
}

auto db_cache::invalidate_on_rollback(
    txfile::transaction::offset_type off,
    cycle_ptr::cycle_gptr<const domain> dom,
    shared_resource_allocator<std::byte> alloc)
-> std::shared_ptr<tx_op> {
  return allocate_tx_op(
      alloc,
      nullptr,
      [self=shared_from_this(this), off, dom]() {
        self->invalidate(off, dom);
      });
}

void db_cache::invalidate_on_rollback(
    txfile::transaction::offset_type off,
    cycle_ptr::cycle_gptr<const domain> dom,
    tx_op_collection& ops) {
  ops.push_back(invalidate_on_rollback(std::move(off), std::move(dom), ops.get_allocator()));
}


db_cache::domain::~domain() noexcept = default;


db_cache::cache_obj::~cache_obj() noexcept = default;


} /* namespace monsoon::tx::detail */
