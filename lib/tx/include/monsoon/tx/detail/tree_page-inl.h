#ifndef MONSOON_TX_DETAIL_TREE_PAGE_INL_H
#define MONSOON_TX_DETAIL_TREE_PAGE_INL_H

#include <algorithm>
#include <iterator>
#include <utility>
#include <monsoon/tx/tree_error.h>
#include <boost/endian/conversion.hpp>

namespace monsoon::tx::detail {


inline abstract_tree_elem::abstract_tree_elem(cycle_ptr::cycle_gptr<abstract_leaf_page> parent)
: parent_(*this, std::move(parent))
{}


inline abstract_node_page::abstract_node_page(cycle_ptr::cycle_gptr<abstract_node_page> parent, std::shared_ptr<const tree_cfg> cfg, std::uint64_t off) noexcept
: off_(off),
  parent_(std::move(parent)),
  cfg_(std::move(cfg))
{}


template<typename Key, typename Val>
inline tree_elem<Key, Val>::tree_elem(cycle_ptr::cycle_gptr<leaf_page<Key, Val>> parent, const Key& key, const Val& val)
: abstract_tree_elem(std::move(parent)),
  key_(key),
  val_(val)
{}

template<typename Key, typename Val>
inline tree_elem<Key, Val>::tree_elem(cycle_ptr::cycle_gptr<leaf_page<Key, Val>> parent, Key&& key, Val&& val)
: abstract_tree_elem(std::move(parent)),
  key_(std::move(key)),
  val_(std::move(val))
{}

template<typename Key, typename Val>
inline tree_elem<Key, Val>::tree_elem(cycle_ptr::cycle_gptr<leaf_page<Key, Val>> parent, boost::asio::const_buffer buf)
: abstract_tree_elem(std::move(parent)),
  key_(boost::asio::const_buffer(buf.data(), std::min(buf.size(), Key::SIZE))),
  val_(buf + Key::SIZE)
{}

template<typename Key, typename Val>
inline void tree_elem<Key, Val>::encode(boost::asio::mutable_buffer buf) const {
  key_.encode(boost::asio::mutable_buffer(buf.data(), std::min(buf.size(), Key::SIZE)));
  val_.encode(buf + Key::SIZE);
}

template<typename Key, typename Val>
inline auto tree_elem<Key, Val>::lock_parent_for_read() const
-> std::tuple<cycle_ptr::cycle_gptr<leaf_page<Key, Val>>, std::shared_lock<std::shared_mutex>> {
  cycle_ptr::cycle_gptr<abstract_leaf_page> p;
  std::shared_lock<std::shared_mutex> lck;
  std::tie(p, lck) = this->abstract_tree_elem::lock_parent_for_read();

#ifdef NDEBUG
  cycle_ptr::cycle_gptr<leaf_page<Key, Val>> casted_p =
      std::static_pointer_cast<leaf_page<Key, Val>>(p);
#else
  cycle_ptr::cycle_gptr<leaf_page<Key, Val>> casted_p =
      std::dynamic_pointer_cast<leaf_page<Key, Val>>(p);
  assert(casted_p != nullptr); // Means the dynamic type wasn't found.
#endif

  return std::make_tuple(std::move(casted_p), std::move(lck));
}

template<typename Key, typename Val>
inline auto tree_elem<Key, Val>::lock_parent_for_write() const
-> std::tuple<cycle_ptr::cycle_gptr<leaf_page<Key, Val>>, std::unique_lock<std::shared_mutex>> {
  cycle_ptr::cycle_gptr<abstract_leaf_page> p;
  std::unique_lock<std::shared_mutex> lck;
  std::tie(p, lck) = this->abstract_tree_elem::lock_parent_for_write();

#ifdef NDEBUG
  cycle_ptr::cycle_gptr<leaf_page<Key, Val>> casted_p =
      std::static_pointer_cast<leaf_page<Key, Val>>(p);
#else
  cycle_ptr::cycle_gptr<leaf_page<Key, Val>> casted_p =
      std::dynamic_pointer_cast<leaf_page<Key, Val>>(p);
  assert(casted_p != nullptr); // Means the dynamic type wasn't found.
#endif

  return std::make_tuple(std::move(casted_p), std::move(lck));
}


template<typename Key, typename Val>
template<typename... Augments>
inline leaf_page<Key, Val>::leaf_page(
    cycle_ptr::cycle_gptr<node_page<Key, Val, Augments...>> parent,
    std::shared_ptr<const tree_cfg> cfg,
    std::uint64_t off) noexcept
: abstract_leaf_page(std::move(parent), std::move(cfg), off)
{}

template<typename Key, typename Val>
inline void leaf_page<Key, Val>::decode(const txfile::transaction& t) {
  std::uint32_t count;

  auto buffer_storage = std::vector<std::uint8_t>(cfg_->items_per_leaf_page * elem_type::SIZE + sizeof(count));
  // Read buffer in local context.
  {
    monsoon::io::fd::offset_type off = off_;
    auto buf = boost::asio::buffer(buffer_storage);
    while (buf.size() > 0) {
      const auto rlen = t.read_at(off, buf.data(), buf.size());
      if (rlen == 0) throw tree_error("unable to read leaf page");
      buf += rlen;
      off += rlen;
    }
  }

  // Now use the buffer.
  boost::asio::const_buffer buf = boost::asio::buffer(buffer_storage);
  // Read how many items are used on this page.
  boost::asio::buffer_copy(boost::asio::buffer(&count, sizeof(count)), buf);
  buf += sizeof(count);
  boost::endian::big_to_native_inplace(count);
  if (count > cfg_->items_per_leaf_page) throw tree_error("too many items on leaf page");

  // Decode all used elements.
  elems_.reserve(count);
  std::generate_n(
      std::back_inserter(elems_), count,
      [&buf, this]() {
        auto new_elem_buf = boost::asio::buffer(buf.data(), std::min(elem_type::SIZE, buf.size()));
        buf += elem_type::SIZE;
        return cycle_ptr::make_cycle<elem_type>(this->shared_from_this(this), std::move(new_elem_buf));
      });
}

template<typename Key, typename Val>
inline void leaf_page<Key, Val>::encode(txfile::transaction& t) const {
  std::uint32_t count = elems_.size();
  boost::endian::native_to_big_inplace(count);

  auto buffer_storage = std::vector<std::uint8_t>(cfg_->items_per_leaf_page * elem_type::SIZE + sizeof(count));
  // Fill the buffer.
  {
    auto buf = boost::asio::buffer(buffer_storage);

    // Write the item count.
    boost::asio::buffer_copy(buf, boost::asio::buffer(&count, sizeof(count)));
    buf += sizeof(count);

    std::for_each(
        elems_.begin(), elems_.end(),
        [&buf, this](const auto& elem_ptr) {
          auto elem_buf = boost::asio::buffer(buf.data(), std::min(elem_type::SIZE, buf.size()));
          buf += elem_type::SIZE;
          elem_ptr->encode(std::move(elem_buf));
        });
  }

  // Write the buffer.
  {
    boost::asio::const_buffer buf = boost::asio::buffer(buffer_storage);
    monsoon::io::fd::offset_type off = off_;
    while (buf.size() > 0) {
      const auto wlen = t.write_at(off, buf.data(), buf.size());
      if (wlen == 0) throw tree_error("unable to write");
      off += wlen;
      buf += wlen;
    }
  }
}


} /* namespace monsoon::tx::detail */

#endif /* MONSOON_TX_DETAIL_TREE_PAGE_INL_H */
