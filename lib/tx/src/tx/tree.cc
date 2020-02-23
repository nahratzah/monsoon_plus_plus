#include <monsoon/tx/tree.h>

namespace monsoon::tx {


abstract_tree::abstract_tx_object::abstract_tx_object(db::transaction& tx, cycle_ptr::cycle_gptr<abstract_tree> self)
: tx_(tx),
  self_(std::move(self)),
  pending_create_(cycle_ptr::cycle_allocator<std::allocator<cycle_ptr::cycle_member_ptr<monsoon::tx::detail::abstract_tree_elem>>>(*this)),
  pending_delete_(cycle_ptr::cycle_allocator<std::allocator<cycle_ptr::cycle_member_ptr<monsoon::tx::detail::abstract_tree_elem>>>(*this)),
  must_exist_during_commit_(cycle_ptr::cycle_allocator<std::allocator<cycle_ptr::cycle_member_ptr<monsoon::tx::detail::abstract_tree_elem>>>(*this))
{}

abstract_tree::abstract_tx_object::~abstract_tx_object() noexcept = default;

void abstract_tree::abstract_tx_object::do_commit(std::uint64_t commit_id, txfile::transaction& tx) {
  // TODO: implement
}

void abstract_tree::abstract_tx_object::do_rollback() noexcept {
  // TODO: implement
}


} /* namespace monsoon::tx */
