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

inline auto abstract_tree::begin() -> abstract_tree_iterator {
  return abstract_tree_iterator(shared_from_this(this), first_element_());
}

inline auto abstract_tree::end() -> abstract_tree_iterator {
  return abstract_tree_iterator(shared_from_this(this), nullptr);
}

template<typename LockType>
inline void abstract_tree::with_equal_range_(cheap_fn_ref<void(abstract_tree_iterator, abstract_tree_iterator)> cb, const abstract_tree_page_branch_key& key) {
  using leaf_locks_type = std::vector<std::pair<cycle_ptr::cycle_gptr<tree_page_leaf>, LockType>>;
  leaf_locks_type leaf_locks;

  // Local scope to populate leaf_locks.
  {
    cycle_ptr::cycle_gptr<tree_page_branch> parent_page;
    cycle_ptr::cycle_gptr<abstract_tree_page> child_page;
    std::shared_lock<std::shared_mutex> parent_lock{ mtx_ };

    if (root_off_ == 0) {
      cb(end(), end());
      return;
    }

    child_page = get(root_off_);
    // Descend down branches.
    while (auto branch_page = std::dynamic_pointer_cast<tree_page_branch>(child_page)) {
      std::tie(parent_page, parent_lock) = std::forward_as_tuple(branch_page, std::shared_lock<std::shared_mutex>(branch_page->mtx_));

      const auto key_range = std::equal_range(branch_page->keys_.begin(), branch_page->keys_.end(), &key,
          [this](const auto& x, const auto& y) -> bool {
            return less_cb(*x, *y);
          });
      if (key_range.first == key_range.second) [[likely]] { // Single page descent.
        child_page = get(branch_page->elems_[key_range.first - branch_page->keys_.begin()]->off);
      } else {
        // We need multiple child pages to describe the range.
        // Use local scoped variables to find the page of the lower bound.
        {
          cycle_ptr::cycle_gptr<tree_page_branch> m_parent_page = branch_page;
          std::shared_lock<std::shared_mutex> m_parent_lock;
          child_page = get(branch_page->elems_[key_range.first - branch_page->keys_.begin()]->off);
          while (auto m_branch_page = std::dynamic_pointer_cast<tree_page_branch>(child_page)) {
            std::tie(m_parent_page, m_parent_lock) = std::forward_as_tuple(m_branch_page, std::shared_lock<std::shared_mutex>(m_branch_page->mtx_));
            const auto i = std::lower_bound(m_branch_page->keys_.begin(), m_branch_page->keys_.end(), &key,
                [this](const auto& x, const auto& y) -> bool {
                  return less_cb(*x, *y);
                });
            child_page = get(m_branch_page->elems_[i - m_branch_page->keys_.begin()]->off);
          }

          // Lock and remember the first leaf page.
          leaf_locks.emplace_back(
              boost::polymorphic_pointer_downcast<tree_page_leaf>(child_page),
              child_page->mtx_);
        }

        child_page = get(branch_page->elems_[key_range.first - branch_page->keys_.begin()]->off);
        while ((branch_page = std::dynamic_pointer_cast<tree_page_branch>(child_page)) != nullptr) {
          std::tie(parent_page, parent_lock) = std::forward_as_tuple(branch_page, std::shared_lock<std::shared_mutex>(branch_page->mtx_));

          const auto i = std::upper_bound(branch_page->keys_.begin(), branch_page->keys_.end(), &key,
              [this](const auto& x, const auto& y) -> bool {
                return less_cb(*x, *y);
              });
          child_page = get(branch_page->elems_[i - branch_page->keys_.begin()]->off);
        }

        // Insert all page up to the child page.
        // Note: we're not inserting child page, as that is handled at the end of the loop.
        for (cycle_ptr::cycle_gptr<tree_page_leaf> successor = leaf_locks.back().first->next();
            successor != child_page;
            successor = successor->next())
          leaf_locks.emplace_back(successor, successor->mtx_);
        break; // Break out of the loop as we've completed the decent.
      }
    }
    // At this point:
    // - child_page is not in leaf_locks
    // - child_page is a leaf
    // - child_page holds the upper bound of the range
    // - parent_page and parent_lock prevent any modifications
    // - leaf_locks holds all pages up to, but excluding child_page
    assert(child_page != nullptr);
    leaf_locks.emplace_back(boost::polymorphic_pointer_cast<tree_page_leaf>(child_page), child_page->mtx_);
  }

  // We made it all the way to a single leaf page.
  // Find the lower bound element.
  typename leaf_locks_type::iterator page_iter = leaf_locks.begin();
  tree_page_leaf::elems_vector::iterator elem_iter;
  cycle_ptr::cycle_gptr<abstract_tree_elem> b;
  // Find the lower bound element on the first leaf.
  elem_iter = std::find_if(
      page_iter->first->elems_.begin(), page_iter->first->elems_.end(),
      [this, &key](const auto& ptr) -> bool {
        return ptr != nullptr && !less_cb(*ptr, key);
      });
  if (elem_iter != page_iter->first->elems_.end()) b = *elem_iter;
  if (b == nullptr) {
    // For subsequent page, we know the less-than test would always pass, so we elide it.
    for (++page_iter;
        page_iter != leaf_locks.end();
        ++page_iter) {
      elem_iter = std::find_if(
          page_iter->first->elems_.begin(), page_iter->first->elems_.end(),
          [](const auto& ptr) -> bool { return ptr != nullptr; });
      if (elem_iter != page_iter->first->elems_.end()) b = *elem_iter;
      // We must break manually: if we would use the for loop,
      // the page_iter would advance before the invariant check was evaluated.
      if (b != nullptr) break;
    }
  }

  if (b == nullptr) {
    // No lower bound found in the entire range.
    // We know it is going to be an empty range.
    // We'll just have to find the next element to use as both lower and upper bounds.
    while (auto next_leaf = leaf_locks.back().first->next()) {
      leaf_locks.emplace_back(next_leaf, next_leaf->mtx_); // Invalidates page_iter.
      for (const auto& elem_ptr : next_leaf->elems_) {
        if (elem_ptr != nullptr) {
          cb(
              abstract_tree_iterator(shared_from_this(this), elem_ptr),
              abstract_tree_iterator(shared_from_this(this), elem_ptr));
          return;
        }
      }
    }
    // There were no successors at all.
    cb(end(), end());
    return;
  }

  // We know the lower bound will be on the last page,
  // and that it is at or after b.
  // Which means that if we are on the last page, we can use b as a starting point.
  if (std::next(page_iter) != leaf_locks.end()) {
    page_iter = std::prev(leaf_locks.end());
    elem_iter = page_iter->first->elems_.begin();
  }
  // Find the upper bound.
  cycle_ptr::cycle_gptr<abstract_tree_elem> e;
  elem_iter = std::find_if(
      page_iter->first->elems_.begin(), page_iter->first->elems_.end(),
      [this, &key](const auto& ptr) -> bool {
        return ptr != nullptr && less_cb(key, *ptr);
      });
  if (elem_iter != page_iter->first->elems_.end()) e = *elem_iter;

  if (e == nullptr) {
    // No upper bound found in the entire range.
    // We'll just have to find the next element to use as upper bound.
    while (auto next_leaf = leaf_locks.back().first->next()) {
      leaf_locks.emplace_back(next_leaf, next_leaf->mtx_); // Invalidates page_iter.
      for (const auto& elem_ptr : next_leaf->elems_) {
        if (elem_ptr != nullptr) {
          cb(
              abstract_tree_iterator(shared_from_this(this), std::move(b)),
              abstract_tree_iterator(shared_from_this(this), elem_ptr));
          return;
        }
      }
    }
    // There were no successors at all.
    cb(
        abstract_tree_iterator(shared_from_this(this), std::move(b)),
        end());
    return;
  }

  cb(
      abstract_tree_iterator(shared_from_this(this), std::move(b)),
      abstract_tree_iterator(shared_from_this(this), std::move(e)));
}

template<typename LockType>
inline void abstract_tree::with_for_each_(cheap_fn_ref<void(cycle_ptr::cycle_gptr<abstract_tree_elem>)> cb) {
  cycle_ptr::cycle_gptr<tree_page_leaf> leaf_page;
  LockType leaf_lock;

  {
    cycle_ptr::cycle_gptr<tree_page_branch> parent_page;
    cycle_ptr::cycle_gptr<abstract_tree_page> child_page;
    std::shared_lock<std::shared_mutex> parent_lock{ mtx_ };

    if (root_off_ == 0) return; // Empty tree.

    child_page = get(root_off_);
    // Descend down branches.
    while (auto branch_page = std::dynamic_pointer_cast<tree_page_branch>(child_page)) {
      std::tie(parent_page, parent_lock) = std::forward_as_tuple(branch_page, std::shared_lock<std::shared_mutex>(branch_page->mtx_));
      child_page = get(branch_page->elems_[0]->off);
    }
    leaf_lock = LockType(child_page->mtx_);
    leaf_page = boost::polymorphic_pointer_downcast<tree_page_leaf>(child_page);
  }

  do {
    std::for_each(
        leaf_page->elems_.begin(), leaf_page->elems_.end(),
        [&cb](const auto& ptr) {
          if (ptr != nullptr) cb(ptr);
        });

    // Advance to the next page.
    auto next_leaf_page = leaf_page->next();
    std::tie(leaf_page, leaf_lock) = std::forward_as_tuple(next_leaf_page, LockType(next_leaf_page->mtx_));
  } while (leaf_page != nullptr);
}


inline abstract_tree_page::abstract_tree_page(cycle_ptr::cycle_gptr<abstract_tree> tree)
: tree_(tree),
  cfg(tree->cfg)
{}


inline abstract_tree_elem::abstract_tree_elem(cycle_ptr::cycle_gptr<tree_page_leaf> parent)
: parent_(*this, std::move(parent)),
  pred_(*this),
  succ_(*this)
{}


inline auto abstract_tx_aware_tree_elem::is_never_visible() const noexcept -> bool {
  return this->tx_aware_data::is_never_visible();
}


inline abstract_tree_iterator::abstract_tree_iterator(cycle_ptr::cycle_gptr<abstract_tree> tree, cycle_ptr::cycle_gptr<abstract_tree_elem> elem) noexcept
: tree_(std::move(tree)),
  elem_(std::move(elem))
{}

inline auto abstract_tree_iterator::get() const noexcept -> pointer {
  return elem_;
}

inline auto abstract_tree_iterator::operator*() const noexcept -> reference {
  assert(elem_ != nullptr);
  return *elem_;
}

inline auto abstract_tree_iterator::operator->() const noexcept -> const pointer& {
  assert(elem_ != nullptr);
  return elem_;
}

inline auto abstract_tree_iterator::operator++() -> abstract_tree_iterator& {
  assert(tree_ != nullptr && elem_ != nullptr);
  elem_ = elem_->next();
  return *this;
}

inline auto abstract_tree_iterator::operator--() -> abstract_tree_iterator& {
  assert(tree_ != nullptr && elem_ != nullptr);
  elem_ = (elem_ == nullptr
      ? tree_->last_element_()
      : elem_->prev());
  return *this;
}

inline auto abstract_tree_iterator::operator++(int) -> abstract_tree_iterator {
  abstract_tree_iterator copy = *this;
  ++*this;
  return copy;
}

inline auto abstract_tree_iterator::operator--(int) -> abstract_tree_iterator {
  abstract_tree_iterator copy = *this;
  --*this;
  return copy;
}

inline auto abstract_tree_iterator::operator==(const abstract_tree_iterator& y) const noexcept -> bool {
  return tree_ == y.tree_ && elem_ == y.elem_;
}

inline auto abstract_tree_iterator::operator!=(const abstract_tree_iterator& y) const noexcept -> bool {
  return !(*this == y);
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
