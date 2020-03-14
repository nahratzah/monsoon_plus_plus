#ifndef MONSOON_TX_DETAIL_TREE_PAGE_INL_H
#define MONSOON_TX_DETAIL_TREE_PAGE_INL_H

#include <monsoon/tx/detail/tree_spec.h>
#include <algorithm>
#include <cassert>
#include <iterator>
#include <numeric>
#include <utility>
#include <boost/endian/conversion.hpp>
#include <boost/polymorphic_pointer_cast.hpp>
#include <objpipe/of.h>

namespace monsoon::tx::detail {


inline abstract_tree::abstract_tree(allocator_type alloc)
: allocator(std::move(alloc))
{}


inline abstract_tree_page::abstract_tree_page(cycle_ptr::cycle_gptr<abstract_tree> tree)
: tree_(tree),
  cfg(tree->cfg)
{}


inline abstract_tree_elem::abstract_tree_elem(cycle_ptr::cycle_gptr<tree_page_leaf> parent)
: parent_(*this, std::move(parent))
{}


inline auto abstract_tx_aware_tree_elem::is_never_visible() const noexcept -> bool {
  return this->tx_aware_data::is_never_visible();
}


template<typename Key, typename Val, typename... Augments>
inline tree_impl<Key, Val, Augments...>::~tree_impl() noexcept = default;

template<typename Key, typename Val, typename... Augments>
inline auto tree_impl<Key, Val, Augments...>::compute_augment_(std::uint64_t off, const std::vector<cycle_ptr::cycle_gptr<const abstract_tree_elem>>& elems) const -> std::shared_ptr<abstract_tree_page_branch_elem> {
  std::tuple<Augments...> augments = objpipe::of(std::cref(elems))
      .iterate()
      .filter([](const auto& elem_ptr) -> bool { return elem_ptr != nullptr; })
      .transform(
          [](const auto& elem_ptr) -> cycle_ptr::cycle_gptr<const tree_elem<Key, Val, Augments...>> {
            return boost::polymorphic_pointer_downcast<const tree_elem<Key, Val, Augments...>>(elem_ptr);
          })
      .transform(
          [](const auto& elem_ptr) -> std::tuple<Augments...> {
            return std::make_tuple(Augments(elem_ptr->key_, elem_ptr->val_)...);
          })
      .reduce(&tree_impl::augment_combine_)
      .value_or(std::tuple<Augments...>());

  return std::allocate_shared<abstract_tree_page_branch_elem>(off, std::move(augments));
}

template<typename Key, typename Val, typename... Augments>
inline auto tree_impl<Key, Val, Augments...>::compute_augment_(std::uint64_t off, const std::vector<std::shared_ptr<const abstract_tree_page_branch_elem>>& elems) const -> std::shared_ptr<abstract_tree_page_branch_elem> {
  std::tuple<Augments...> augments = objpipe::of(std::cref(elems))
      .iterate()
      .filter([](const auto& elem_ptr) -> bool { return elem_ptr != nullptr; })
      .transform(
          [](const auto& elem_ptr) -> cycle_ptr::cycle_gptr<const tree_page_branch_elem<Key, Val, Augments...>> {
            return boost::polymorphic_pointer_downcast<const tree_elem<Key, Val, Augments...>>(elem_ptr);
          })
      .transform(
          [](const auto& elem_ptr) -> const std::tuple<Augments...>& {
            return elem_ptr->augments;
          })
      .reduce(&tree_impl::augment_combine_)
      .value_or(std::tuple<Augments...>());

  return std::allocate_shared<abstract_tree_page_branch_elem>(off, std::move(augments));
}

template<typename Key, typename Val, typename... Augments>
inline auto tree_impl<Key, Val, Augments...>::augment_combine_(const std::tuple<Augments...>& x, const std::tuple<Augments...>& y) -> std::tuple<Augments...> {
  return augment_combine_seq_(x, y, std::index_sequence_for<Augments...>());
}

template<typename Key, typename Val, typename... Augments>
template<std::size_t... Idxs>
inline auto tree_impl<Key, Val, Augments...>::augment_combine_seq_(const std::tuple<Augments...>& x, const std::tuple<Augments...>& y, [[maybe_unused]] std::index_sequence<Idxs...> seq) -> std::tuple<Augments...> {
  return std::make_tuple(Augments::merge(std::get<Idxs>(x), std::get<Idxs>(y))...);
}

template<typename Key, typename Val, typename... Augments>
auto tree_impl<Key, Val, Augments...>::allocate_elem_(cycle_ptr::cycle_gptr<tree_page_leaf> parent) const -> cycle_ptr::cycle_gptr<abstract_tree_elem> {
  return cycle_ptr::allocate_cycle<tree_elem<Key, Val, Augments...>>(allocator, std::move(parent));
}

template<typename Key, typename Val, typename... Augments>
auto tree_impl<Key, Val, Augments...>::allocate_branch_elem_() const -> std::shared_ptr<abstract_tree_page_branch_elem> {
  return std::allocate_shared<tree_page_branch_elem<Augments...>>(allocator);
}

template<typename Key, typename Val, typename... Augments>
auto tree_impl<Key, Val, Augments...>::allocate_branch_key_() const -> std::shared_ptr<abstract_tree_page_branch_key> {
  return std::allocate_shared<tree_page_branch_key<Key>>(allocator);
}


template<typename Key, typename Val, typename... Augments>
inline tree_elem<Key, Val, Augments...>::tree_elem(cycle_ptr::cycle_gptr<tree_page_leaf> parent, const Key& key, const Val& val)
: abstract_tree_elem(std::move(parent)),
  key_(key),
  val_(val)
{}

template<typename Key, typename Val, typename... Augments>
inline tree_elem<Key, Val, Augments...>::tree_elem(cycle_ptr::cycle_gptr<tree_page_leaf> parent, Key&& key, Val&& val)
: abstract_tree_elem(std::move(parent)),
  key_(std::move(key)),
  val_(std::move(val))
{}

template<typename Key, typename Val, typename... Augments>
inline tree_elem<Key, Val, Augments...>::tree_elem(cycle_ptr::cycle_gptr<tree_page_leaf> parent)
: abstract_tree_elem(std::move(parent))
{}

template<typename Key, typename Val, typename... Augments>
inline void tree_elem<Key, Val, Augments...>::encode(boost::asio::mutable_buffer buf) const {
  key_.encode(boost::asio::mutable_buffer(buf.data(), std::min(buf.size(), Key::SIZE)));
  val_.encode(buf + Key::SIZE);
}

template<typename Key, typename Val, typename... Augments>
inline auto tree_elem<Key, Val, Augments...>::branch_key_(abstract_tree::allocator_type alloc) const -> std::shared_ptr<abstract_tree_page_branch_key> {
  return std::allocate_shared<tree_page_branch_key<Key>>(alloc, key_);
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
inline auto tree_page_branch_elem<Augments...>::augment_offset([[maybe_unused]] std::size_t idx) -> std::size_t {
  assert(idx <= sizeof...(Augments));  // We allow idx to be the 'end' index.

  if constexpr(sizeof...(Augments) == 0) {
    return sizeof(off);
  } else {
    const std::array<std::size_t, sizeof...(Augments)> sizes{{ Augments::SIZE... }};
    return std::accumulate(sizes.begin(), sizes.begin() + idx, sizeof(off));
  }
}


template<typename Key>
inline tree_page_branch_key<Key>::tree_page_branch_key(Key&& key)
    noexcept(std::is_nothrow_move_constructible_v<Key>)
: key(std::move(key))
{}

template<typename Key>
inline tree_page_branch_key<Key>::tree_page_branch_key(const Key& key)
    noexcept(std::is_nothrow_copy_constructible_v<Key>)
: key(key)
{}

template<typename Key>
inline void tree_page_branch_key<Key>::decode(boost::asio::const_buffer buf) {
  assert(buf.size() >= Key::SIZE);
  key.decode(buf);
}

template<typename Key>
inline void tree_page_branch_key<Key>::encode(boost::asio::mutable_buffer buf) const {
  assert(buf.size() >= Key::SIZE);
  key.encode(buf);
}


} /* namespace monsoon::tx::detail */

#endif /* MONSOON_TX_DETAIL_TREE_PAGE_INL_H */
