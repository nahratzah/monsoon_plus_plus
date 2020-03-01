#ifndef MONSOON_TX_DETAIL_TREE_PAGE_H
#define MONSOON_TX_DETAIL_TREE_PAGE_H

#include <monsoon/tx/txfile.h>
#include <monsoon/tx/tx_aware_data.h>
#include <monsoon/tx/detail/tree_cfg.h>
#include <shared_mutex>
#include <tuple>
#include <type_traits>
#include <vector>
#include <boost/asio/buffer.hpp>
#include <cycle_ptr/cycle_ptr.h>

namespace monsoon::tx::detail {


class abstract_tree_elem;
class abstract_leaf_page;
class abstract_node_page;

template<typename Key, typename Val>
class tree_elem;

template<typename Key, typename Val>
class leaf_page;

template<typename Key, typename Val, typename... Augments>
class node_page;


class abstract_tree_elem
: protected cycle_ptr::cycle_base,
  public tx_aware_data
{
  friend abstract_leaf_page;

  protected:
  abstract_tree_elem(cycle_ptr::cycle_gptr<abstract_leaf_page> parent);
  virtual ~abstract_tree_elem() noexcept;

  public:
  virtual void encode(boost::asio::mutable_buffer buf) const = 0;

  ///\brief Acquire the parent and lock it for read.
  ///\details May only be called without a lock on this element.
  ///\note While the parent is locked, this element can't change parent.
  auto lock_parent_for_read() const
  -> std::tuple<cycle_ptr::cycle_gptr<abstract_leaf_page>, std::shared_lock<std::shared_mutex>>;

  ///\brief Acquire the parent and lock it for write.
  ///\details May only be called without a lock on this element.
  ///\note While the parent is locked, this element can't change parent.
  auto lock_parent_for_write() const
  -> std::tuple<cycle_ptr::cycle_gptr<abstract_leaf_page>, std::unique_lock<std::shared_mutex>>;

  private:
  cycle_ptr::cycle_member_ptr<abstract_leaf_page> parent_;
};


class abstract_node_page
: protected cycle_ptr::cycle_base
{
  friend abstract_leaf_page;

  protected:
  abstract_node_page(cycle_ptr::cycle_gptr<abstract_node_page> parent, std::shared_ptr<const tree_cfg> cfg, std::uint64_t off) noexcept;
  virtual ~abstract_node_page() noexcept;

  public:
  virtual void decode(const txfile::transaction& t) = 0;
  virtual void encode(txfile::transaction& t) const = 0;

  protected:
  mutable std::shared_mutex mtx_;
  std::uint64_t off_ = 0;

  private:
  std::uint64_t parent_off_;
  cycle_ptr::cycle_weak_ptr<abstract_node_page> parent_;

  protected:
  std::shared_ptr<const tree_cfg> cfg_;
};


class abstract_leaf_page
: public abstract_node_page
{
  friend abstract_tree_elem;

  protected:
  using abstract_node_page::abstract_node_page;
  virtual ~abstract_leaf_page() noexcept;
};


template<typename Key, typename Val>
class tree_elem
: public abstract_tree_elem
{
  public:
  static constexpr std::size_t SIZE = tx_aware_data::TX_AWARE_SIZE + Key::SIZE + Val::SIZE;

  tree_elem(cycle_ptr::cycle_gptr<leaf_page<Key, Val>> parent, const Key& key, const Val& val);
  tree_elem(cycle_ptr::cycle_gptr<leaf_page<Key, Val>> parent, Key&& key, Val&& val);
  explicit tree_elem(cycle_ptr::cycle_gptr<leaf_page<Key, Val>> parent, boost::asio::const_buffer buf);

  void encode(boost::asio::mutable_buffer buf) const override;

  ///\brief Acquire the parent and lock it for read.
  ///\details May only be called without a lock on this element.
  ///\note While the parent is locked, this element can't change parent.
  auto lock_parent_for_read() const
  -> std::tuple<cycle_ptr::cycle_gptr<leaf_page<Key, Val>>, std::shared_lock<std::shared_mutex>>;

  ///\brief Acquire the parent and lock it for write.
  ///\details May only be called without a lock on this element.
  ///\note While the parent is locked, this element can't change parent.
  auto lock_parent_for_write() const
  -> std::tuple<cycle_ptr::cycle_gptr<leaf_page<Key, Val>>, std::unique_lock<std::shared_mutex>>;

  private:
  Key key_;
  Val val_;
};


template<typename Key, typename Val>
class leaf_page
: public abstract_leaf_page
{
  public:
  using elem_type = tree_elem<Key, Val>;

  leaf_page() = default;

  template<typename... Augments>
  leaf_page(
      cycle_ptr::cycle_gptr<node_page<Key, Val, Augments...>> parent,
      std::shared_ptr<const tree_cfg> cfg,
      std::uint64_t off) noexcept;

  void decode(const txfile::transaction& t) override;
  void encode(txfile::transaction& t) const override;

  private:
  std::vector<cycle_ptr::cycle_member_ptr<elem_type>> elems_;
};


template<typename Key, typename Val, typename... Augments>
class node_page
: public abstract_node_page
{
};


} /* namespace monsoon::tx::detail */

#include "tree_page-inl.h"

#endif /* MONSOON_TX_DETAIL_TREE_PAGE_H */
