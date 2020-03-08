#include <monsoon/tx/detail/commit_manager.h>
#include <monsoon/tx/detail/commit_manager_impl.h>
#include <monsoon/io/rw.h>
#include <boost/endian/conversion.hpp>

namespace monsoon::tx::detail {


commit_manager::~commit_manager() noexcept = default;

auto commit_manager::allocate(const txfile& f, monsoon::io::fd::offset_type off, allocator_type alloc) -> std::shared_ptr<commit_manager> {
  std::uint32_t magic;

  {
    auto tx = f.begin();
    monsoon::io::read_at(tx, off, &magic, sizeof(magic));
    boost::endian::big_to_native_inplace(magic);
  }

  switch (magic) {
    default:
      throw std::runtime_error("invalid magic for commit manager");
    case commit_manager_impl::magic:
      return commit_manager_impl::allocate(f, off, std::move(alloc));
  }
}

auto commit_manager::get_tx_commit_id() const -> commit_id {
  return get_tx_commit_id(alloc_);
}

auto commit_manager::get_tx_commit_id(allocator_type tx_alloc) const -> commit_id {
  return do_get_tx_commit_id_(std::move(tx_alloc));
}

auto commit_manager::prepare_commit(txfile& f) -> write_id {
  return prepare_commit(f, alloc_);
}

auto commit_manager::prepare_commit(txfile& f, allocator_type tx_alloc) -> write_id {
  return do_prepare_commit_(f, std::move(tx_alloc));
}


} /* namespace monsoon::tx::detail */
