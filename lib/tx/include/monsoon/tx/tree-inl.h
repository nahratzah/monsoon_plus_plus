#ifndef MONSOON_TX_TREE_INL_H
#define MONSOON_TX_TREE_INL_H

#include <cassert>

namespace monsoon::tx {


template<typename Key, typename Val, typename Less, typename... Augments>
inline tree<Key, Val, Less, Augments...>::tx_object::tx_object(db::transaction& tx, cycle_ptr::cycle_gptr<tree> self)
: abstract_tree::abstract_tx_object(tx, std::move(self))
{}


template<typename Key, typename Val, typename Less, typename... Augments>
inline auto tree<Key, Val, Less, Augments...>::tx_object::self() const noexcept -> cycle_ptr::cycle_gptr<tree> {
#ifdef NDEBUG
  return std::static_pointer_cast<tree>(this->abstract_tx_object::self());
#else
  cycle_ptr::cycle_gptr<tree> ptr = std::dynamic_pointer_cast<tree>(this->abstract_tx_object::self());
  assert(ptr != nullptr); // Would only happen if the types are incorrect.
  return ptr;
#endif
}


} /* namespace monsoon::tx */

#endif /* MONSOON_TX_TREE_INL_H */
