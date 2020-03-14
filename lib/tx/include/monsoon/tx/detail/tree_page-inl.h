#ifndef MONSOON_TX_DETAIL_TREE_PAGE_INL_H
#define MONSOON_TX_DETAIL_TREE_PAGE_INL_H

#include <algorithm>
#include <cassert>
#include <iterator>
#include <numeric>
#include <utility>
#include <boost/endian/conversion.hpp>
#include <monsoon/tx/detail/tree_spec.h>

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


inline auto abstract_tx_aware_tree_elem::is_never_visible() const noexcept -> bool {
  return this->tx_aware_data::is_never_visible();
}


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


template<typename... Augments>
inline tree_page_branch_elem<Augments...>::tree_page_branch_elem(std::uint64_t off, std::tuple<Augments...> augments)
    noexcept(std::is_nothrow_move_constructible_v<std::tuple<Augments...>>)
: abstract_tree_page_branch_elem(off),
  augments(std::move(augments))
{}

template<typename... Augments>
inline void tree_page_branch_elem<Augments...>::decode(boost::asio::const_buffer buf) {
  assert(buf.size() >= sum_(sizeof(off), Augments::SIZE...));

  boost::asio::buffer_copy(boost::asio::buffer(&off, sizeof(off)), buf);
  boost::endian::big_to_native_inplace(off);

  decode_idx_seq_(buf, std::index_sequence_for<Augments...>());
}

template<typename... Augments>
inline void tree_page_branch_elem<Augments...>::encode(boost::asio::mutable_buffer buf) const {
  assert(buf.size() >= sum_(sizeof(off), Augments::SIZE...));

  const decltype(off) off_be = boost::endian::native_to_big(off);
  boost::asio::buffer_copy(buf, boost::asio::buffer(&off_be, sizeof(off_be)));

  encode_idx_seq_(buf, std::index_sequence_for<Augments...>());
}

template<typename... Augments>
template<std::size_t Idx0, std::size_t... Idxs>
inline void tree_page_branch_elem<Augments...>::decode_idx_seq_(boost::asio::const_buffer buf, [[maybe_unused]] std::index_sequence<Idx0, Idxs...> seq) {
  this->template decode_idx_<Idx0>(buf);
  this->decode_idx_seq_(buf, std::index_sequence<Idxs...>());
}

template<typename... Augments>
template<std::size_t Idx0, std::size_t... Idxs>
inline void tree_page_branch_elem<Augments...>::encode_idx_seq_(boost::asio::mutable_buffer buf, [[maybe_unused]] std::index_sequence<Idx0, Idxs...> seq) const {
  this->template encode_idx_<Idx0>(buf);
  this->encode_idx_seq_(buf, std::index_sequence<Idxs...>());
}

template<typename... Augments>
inline void tree_page_branch_elem<Augments...>::decode_idx_seq_(boost::asio::const_buffer buf, [[maybe_unused]] std::index_sequence<> seq) noexcept {}

template<typename... Augments>
inline void tree_page_branch_elem<Augments...>::encode_idx_seq_(boost::asio::mutable_buffer buf, [[maybe_unused]] std::index_sequence<> seq) const noexcept {}

template<typename... Augments>
template<std::size_t Idx>
inline void tree_page_branch_elem<Augments...>::decode_idx_(boost::asio::const_buffer buf) {
  using augment_type = std::tuple_element_t<Idx, std::tuple<Augments...>>;

  // Set the buffer to only the range described by this augment.
  assert(buf.size() >= augment_offset(Idx + 1u));
  buf += augment_offset(Idx);
  buf = boost::asio::buffer(buf.data(), augment_type::SIZE);
  // Invoke decoder.
  std::get<Idx>(augments).decode(buf);
}

template<typename... Augments>
template<std::size_t Idx>
inline void tree_page_branch_elem<Augments...>::encode_idx_(boost::asio::mutable_buffer buf) const {
  using augment_type = std::tuple_element_t<Idx, std::tuple<Augments...>>;

  // Set the buffer to only the range described by this augment.
  assert(buf.size() >= augment_offset(Idx + 1u));
  buf += augment_offset(Idx);
  buf = boost::asio::buffer(buf.data(), augment_type::SIZE);
  // Invoke decoder.
  std::get<Idx>(augments).encode(buf);
}

template<typename... Augments>
inline auto tree_page_branch_elem<Augments...>::augment_offset(std::size_t idx) -> std::size_t {
  assert(idx <= sizeof...(Augments));  // We allow idx to be the 'end' index.

  const std::array<std::size_t, sizeof...(Augments)> sizes{{ Augments::SIZE... }};
  return std::accumulate(sizes.begin(), sizes.begin() + idx, sizeof(off));
}


} /* namespace monsoon::tx::detail */

#endif /* MONSOON_TX_DETAIL_TREE_PAGE_INL_H */
