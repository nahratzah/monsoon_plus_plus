#ifndef MONSOON_TX_DETAIL_TREE_PAGE_H
#define MONSOON_TX_DETAIL_TREE_PAGE_H

#include <monsoon/tx/txfile.h>
#include <monsoon/tx/tx_aware_data.h>
#include <monsoon/tx/detail/tree_cfg.h>
#include <monsoon/tx/detail/export_.h>
#include <monsoon/cheap_fn_ref.h>
#include <monsoon/shared_resource_allocator.h>
#include <shared_mutex>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <vector>
#include <boost/asio/buffer.hpp>
#include <cycle_ptr/cycle_ptr.h>
#include <cycle_ptr/allocator.h>

namespace monsoon::tx::detail {


class abstract_tree;
class abstract_tree_elem;
class abstract_tree_page;
class abstract_tree_page_leaf;
class abstract_tree_page_branch;

template<typename Key, typename Val, typename... Augments>
class tree_elem;

template<typename Key, typename Val, typename... Augments>
class tree_page_leaf;

template<typename Key, typename Val, typename... Augments>
class tree_page_branch;


class monsoon_tx_export_ abstract_tree
: public db::db_obj
{
  public:
  ///\brief Allocator used by commit_manager.
  using allocator_type = shared_resource_allocator<std::byte>;
  ///\brief Allocator traits.
  using traits_type = std::allocator_traits<allocator_type>;

  protected:
  explicit abstract_tree(allocator_type alloc = allocator_type());
  ~abstract_tree() noexcept override;

  public:
  const std::shared_ptr<const tree_cfg> cfg;
  allocator_type allocator;

  protected:
  std::uint64_t root_off_ = 0;
};


class monsoon_tx_export_ abstract_tree_page
: protected cycle_ptr::cycle_base
{
  protected:
  explicit abstract_tree_page(cycle_ptr::cycle_gptr<abstract_tree> tree);
  explicit abstract_tree_page(cycle_ptr::cycle_gptr<abstract_tree_page_branch> parent);
  virtual ~abstract_tree_page() noexcept = 0;

  public:
  ///\brief Get the tree owning this page.
  ///\returns Pointer to the tree.
  ///\throws std::bad_weak_ptr If the tree has gone away.
  auto tree() const -> cycle_ptr::cycle_gptr<abstract_tree>;

  ///\brief Decode a page.
  ///\details Uses the magic encoded in \p tx to determine what type of page is stored at the offset.
  ///\param[in] tx Source of bytes on which the decode operation operates.
  ///\param off Offset in \p tx where the page is located.
  ///\param leaf_constructor Functor that return a newly allocated abstract_tree_page_leaf.
  ///\param branch_constructor Functor that return a newly allocated abstract_tree_page_branch.
  ///\return A newly allocated page, that was read from bytes in \p tx at offset \p off and is of the appropriate type.
  static auto decode(
      const txfile::transaction& tx, std::uint64_t off,
      cheap_fn_ref<cycle_ptr::cycle_gptr<abstract_tree_page_leaf>> leaf_constructor,
      cheap_fn_ref<cycle_ptr::cycle_gptr<abstract_tree_page_branch>> branch_constructor)
  -> cycle_ptr::cycle_gptr<abstract_tree_page>;

  virtual void decode(const txfile::transaction& tx, std::uint64_t off) = 0;
  virtual void encode(txfile::transaction& tx) const = 0;

  protected:
  std::uint64_t off_ = 0, parent_off_ = 0;
  mutable std::shared_mutex mtx_;

  private:
  const cycle_ptr::cycle_weak_ptr<abstract_tree> tree_;

  public:
  const std::shared_ptr<const tree_cfg> cfg;
};


class monsoon_tx_export_ abstract_tree_page_leaf
: public abstract_tree_page
{
  friend abstract_tree_elem;

  public:
  static constexpr std::uint32_t magic = 0x2901'c28fU;

  struct header {
    static constexpr std::size_t SIZE = 32;

    std::uint32_t magic;
    std::uint32_t reserved;
    std::uint64_t parent_off;
    std::uint64_t next_sibling_off;
    std::uint64_t prev_sibling_off;

    monsoon_tx_local_ void native_to_big_endian() noexcept;
    monsoon_tx_local_ void big_to_native_endian() noexcept;
    monsoon_tx_local_ void encode(boost::asio::mutable_buffer buf) const;
    monsoon_tx_local_ void decode(boost::asio::const_buffer buf);
  };
  static_assert(sizeof(header) == header::SIZE);

  private:
  using elems_vector = std::vector<
      cycle_ptr::cycle_member_ptr<abstract_tree_elem>,
      cycle_ptr::cycle_allocator<
          abstract_tree::traits_type::rebind_alloc<
              cycle_ptr::cycle_member_ptr<abstract_tree_elem>
          >
      >
  >;

  protected:
  explicit abstract_tree_page_leaf(cycle_ptr::cycle_gptr<abstract_tree> tree);
  explicit abstract_tree_page_leaf(cycle_ptr::cycle_gptr<abstract_tree_page_branch> parent);
  ~abstract_tree_page_leaf() noexcept override;

  void init_empty(std::uint64_t off);
  void decode(const txfile::transaction& tx, std::uint64_t off) override final;
  void encode(txfile::transaction& tx) const override final;

  private:
  virtual auto allocate_elem_(abstract_tree::allocator_type alloc) const -> cycle_ptr::cycle_gptr<abstract_tree_elem> = 0;

  std::uint64_t next_sibling_off_ = 0, prev_sibling_off_ = 0;
  elems_vector elems_;
};


class monsoon_tx_export_ abstract_tree_page_branch
: public abstract_tree_page
{
  public:
  static constexpr std::uint32_t magic = 0x5825'b1f0U;

  struct header {
    static constexpr std::size_t SIZE = 16;

    std::uint32_t magic;
    std::uint32_t size;
    std::uint64_t parent_off;

    monsoon_tx_local_ void native_to_big_endian() noexcept;
    monsoon_tx_local_ void big_to_native_endian() noexcept;
    monsoon_tx_local_ void encode(boost::asio::mutable_buffer buf) const;
    monsoon_tx_local_ void decode(boost::asio::const_buffer buf);
  };
  static_assert(sizeof(header) == header::SIZE);

  protected:
  ///\brief Abstract key interface.
  class key {
    protected:
    key() noexcept = default;
    virtual ~key() noexcept = 0;

    public:
    virtual void decode(boost::asio::const_buffer buf) = 0;
    virtual void encode(boost::asio::mutable_buffer buf) const = 0;
  };

  ///\brief Abstract branch element.
  class elem {
    public:
    static constexpr std::size_t offset_size = sizeof(std::uint64_t);

    protected:
    elem() noexcept = default;
    elem(std::uint64_t off) noexcept : off(off) {}
    virtual ~elem() noexcept = 0;

    public:
    virtual void decode(boost::asio::const_buffer buf) = 0;
    virtual void encode(boost::asio::mutable_buffer buf) const = 0;

    ///\brief Offset of the page this branch element points at.
    std::uint64_t off = 0;
  };

  private:
  using elems_vector = std::vector<
      cycle_ptr::cycle_member_ptr<elem>,
      cycle_ptr::cycle_allocator<
          abstract_tree::traits_type::rebind_alloc<
              cycle_ptr::cycle_member_ptr<elem>
          >
      >
  >;

  using keys_vector = std::vector<
      cycle_ptr::cycle_member_ptr<key>,
      cycle_ptr::cycle_allocator<
          abstract_tree::traits_type::rebind_alloc<
              cycle_ptr::cycle_member_ptr<key>
          >
      >
  >;

  protected:
  explicit abstract_tree_page_branch(cycle_ptr::cycle_gptr<abstract_tree> tree);
  explicit abstract_tree_page_branch(cycle_ptr::cycle_gptr<abstract_tree_page_branch> parent);
  ~abstract_tree_page_branch() noexcept override;

  void decode(const txfile::transaction& tx, std::uint64_t off) override final;
  void encode(txfile::transaction& tx) const override final;

  private:
  virtual auto allocate_elem_(abstract_tree::allocator_type alloc) const -> cycle_ptr::cycle_gptr<elem> = 0;
  virtual auto allocate_key_(abstract_tree::allocator_type alloc) const -> cycle_ptr::cycle_gptr<key> = 0;

  elems_vector elems_;
  keys_vector keys_;
};


class monsoon_tx_export_ abstract_tree_elem
: public tx_aware_data,
  protected cycle_ptr::cycle_base
{
  friend abstract_tree_page_leaf;

  protected:
  abstract_tree_elem(cycle_ptr::cycle_gptr<abstract_tree_page_leaf> parent);
  virtual ~abstract_tree_elem() noexcept = 0;

  public:
  virtual void decode(boost::asio::const_buffer buf) = 0;
  virtual void encode(boost::asio::mutable_buffer buf) const = 0;

  ///\brief Acquire the parent and lock it for read.
  ///\details May only be called without a lock on this element.
  ///\note While the parent is locked, this element can't change parent.
  auto lock_parent_for_read() const
  -> std::tuple<cycle_ptr::cycle_gptr<abstract_tree_page_leaf>, std::shared_lock<std::shared_mutex>>;

  ///\brief Acquire the parent and lock it for write.
  ///\details May only be called without a lock on this element.
  ///\note While the parent is locked, this element can't change parent.
  auto lock_parent_for_write() const
  -> std::tuple<cycle_ptr::cycle_gptr<abstract_tree_page_leaf>, std::unique_lock<std::shared_mutex>>;

  private:
  cycle_ptr::cycle_member_ptr<abstract_tree_page_leaf> parent_;
};


template<typename Key, typename Val, typename... Augments>
class tree_elem
: public abstract_tree_elem
{
  public:
  static constexpr std::size_t SIZE = tx_aware_data::TX_AWARE_SIZE + Key::SIZE + Val::SIZE;

  tree_elem(cycle_ptr::cycle_gptr<tree_page_leaf<Key, Val>> parent, const Key& key, const Val& val);
  tree_elem(cycle_ptr::cycle_gptr<tree_page_leaf<Key, Val>> parent, Key&& key, Val&& val);
  explicit tree_elem(cycle_ptr::cycle_gptr<tree_page_leaf<Key, Val>> parent);

  void decode(boost::asio::const_buffer buf) override;
  void encode(boost::asio::mutable_buffer buf) const override;

  ///\brief Acquire the parent and lock it for read.
  ///\details May only be called without a lock on this element.
  ///\note While the parent is locked, this element can't change parent.
  auto lock_parent_for_read() const
  -> std::tuple<cycle_ptr::cycle_gptr<tree_page_leaf<Key, Val>>, std::shared_lock<std::shared_mutex>>;

  ///\brief Acquire the parent and lock it for write.
  ///\details May only be called without a lock on this element.
  ///\note While the parent is locked, this element can't change parent.
  auto lock_parent_for_write() const
  -> std::tuple<cycle_ptr::cycle_gptr<tree_page_leaf<Key, Val>>, std::unique_lock<std::shared_mutex>>;

  private:
  Key key_;
  Val val_;
};


template<typename Key, typename Val, typename... Augments>
class tree_page_leaf final
: public abstract_tree_page_leaf
{
  public:
  explicit tree_page_leaf(cycle_ptr::cycle_gptr<abstract_tree> tree);
  explicit tree_page_leaf(cycle_ptr::cycle_gptr<tree_page_branch<Key, Val, Augments...>> parent);
  ~tree_page_leaf() noexcept;

  private:
  auto allocate_elem_(abstract_tree::allocator_type alloc) const -> cycle_ptr::cycle_gptr<abstract_tree_elem> override;
};


} /* namespace monsoon::tx::detail */

#include "tree_page-inl.h"

#endif /* MONSOON_TX_DETAIL_TREE_PAGE_H */
