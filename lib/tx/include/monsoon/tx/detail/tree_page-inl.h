#ifndef MONSOON_TX_DETAIL_TREE_PAGE_INL_H
#define MONSOON_TX_DETAIL_TREE_PAGE_INL_H

#include <algorithm>
#include <iterator>
#include <utility>
#include <cassert>

namespace monsoon::tx::detail {


inline abstract_tree::abstract_tree(allocator_type alloc)
: allocator(std::move(alloc))
{}


inline abstract_tree_page::abstract_tree_page(cycle_ptr::cycle_gptr<abstract_tree> tree)
: tree_(tree),
  cfg(tree->cfg)
{}

inline abstract_tree_page::abstract_tree_page(cycle_ptr::cycle_gptr<abstract_tree_page_branch> parent)
: abstract_tree_page(parent->tree())
{}


inline abstract_tree_elem::abstract_tree_elem(cycle_ptr::cycle_gptr<abstract_tree_page_leaf> parent)
: parent_(*this, std::move(parent))
{}


template<typename Key, typename Val, typename... Augments>
inline tree_elem<Key, Val, Augments...>::tree_elem(cycle_ptr::cycle_gptr<tree_page_leaf<Key, Val>> parent, const Key& key, const Val& val)
: abstract_tree_elem(std::move(parent)),
  key_(key),
  val_(val)
{}

template<typename Key, typename Val, typename... Augments>
inline tree_elem<Key, Val, Augments...>::tree_elem(cycle_ptr::cycle_gptr<tree_page_leaf<Key, Val>> parent, Key&& key, Val&& val)
: abstract_tree_elem(std::move(parent)),
  key_(std::move(key)),
  val_(std::move(val))
{}

template<typename Key, typename Val, typename... Augments>
inline tree_elem<Key, Val, Augments...>::tree_elem(cycle_ptr::cycle_gptr<tree_page_leaf<Key, Val>> parent)
: abstract_tree_elem(std::move(parent))
{}

template<typename Key, typename Val, typename... Augments>
inline void tree_elem<Key, Val, Augments...>::encode(boost::asio::mutable_buffer buf) const {
  key_.encode(boost::asio::mutable_buffer(buf.data(), std::min(buf.size(), Key::SIZE)));
  val_.encode(buf + Key::SIZE);
}

template<typename Key, typename Val, typename... Augments>
inline auto tree_elem<Key, Val, Augments...>::lock_parent_for_read() const
-> std::tuple<cycle_ptr::cycle_gptr<tree_page_leaf<Key, Val>>, std::shared_lock<std::shared_mutex>> {
  cycle_ptr::cycle_gptr<abstract_tree_page_leaf> p;
  std::shared_lock<std::shared_mutex> lck;
  std::tie(p, lck) = this->abstract_tree_elem::lock_parent_for_read();

#ifdef NDEBUG
  cycle_ptr::cycle_gptr<tree_page_leaf<Key, Val>> casted_p =
      std::static_pointer_cast<tree_page_leaf<Key, Val>>(p);
#else
  cycle_ptr::cycle_gptr<tree_page_leaf<Key, Val>> casted_p =
      std::dynamic_pointer_cast<tree_page_leaf<Key, Val>>(p);
  assert(casted_p != nullptr); // Means the dynamic type wasn't found.
#endif

  return std::make_tuple(std::move(casted_p), std::move(lck));
}

template<typename Key, typename Val, typename... Augments>
inline auto tree_elem<Key, Val, Augments...>::lock_parent_for_write() const
-> std::tuple<cycle_ptr::cycle_gptr<tree_page_leaf<Key, Val>>, std::unique_lock<std::shared_mutex>> {
  cycle_ptr::cycle_gptr<abstract_tree_page_leaf> p;
  std::unique_lock<std::shared_mutex> lck;
  std::tie(p, lck) = this->abstract_tree_elem::lock_parent_for_write();

#ifdef NDEBUG
  cycle_ptr::cycle_gptr<tree_page_leaf<Key, Val>> casted_p =
      std::static_pointer_cast<tree_page_leaf<Key, Val>>(p);
#else
  cycle_ptr::cycle_gptr<tree_page_leaf<Key, Val>> casted_p =
      std::dynamic_pointer_cast<tree_page_leaf<Key, Val>>(p);
  assert(casted_p != nullptr); // Means the dynamic type wasn't found.
#endif

  return std::make_tuple(std::move(casted_p), std::move(lck));
}


template<typename Key, typename Val, typename... Augments>
inline tree_page_leaf<Key, Val, Augments...>::tree_page_leaf(cycle_ptr::cycle_gptr<abstract_tree> tree)
: abstract_tree_page_leaf(std::move(tree))
{}

template<typename Key, typename Val, typename... Augments>
inline tree_page_leaf<Key, Val, Augments...>::tree_page_leaf(cycle_ptr::cycle_gptr<tree_page_branch<Key, Val, Augments...>> parent)
: abstract_tree_page_leaf(std::move(parent))
{}

template<typename Key, typename Val, typename... Augments>
inline tree_page_leaf<Key, Val, Augments...>::~tree_page_leaf() noexcept = default;

template<typename Key, typename Val, typename... Augments>
inline auto tree_page_leaf<Key, Val, Augments...>::allocate_elem_(abstract_tree::allocator_type alloc) const -> cycle_ptr::cycle_gptr<abstract_tree_elem> {
  return cycle_ptr::allocate_cycle<tree_elem<Key, Val, Augments...>>(alloc, shared_from_this(this));
}


} /* namespace monsoon::tx::detail */

#endif /* MONSOON_TX_DETAIL_TREE_PAGE_INL_H */
