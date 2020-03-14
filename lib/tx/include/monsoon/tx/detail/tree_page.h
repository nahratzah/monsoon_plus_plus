#ifndef MONSOON_TX_DETAIL_TREE_PAGE_H
#define MONSOON_TX_DETAIL_TREE_PAGE_H

#include <monsoon/tx/txfile.h>
#include <monsoon/tx/tx_aware_data.h>
#include <monsoon/tx/detail/tree_cfg.h>
#include <monsoon/tx/detail/export_.h>
#include <monsoon/cheap_fn_ref.h>
#include <monsoon/shared_resource_allocator.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <tuple>
#include <type_traits>
#include <vector>
#include <boost/asio/buffer.hpp>
#include <cycle_ptr/cycle_ptr.h>
#include <cycle_ptr/allocator.h>

namespace monsoon::tx::detail {


class abstract_tree;
class abstract_tree_page;
class abstract_tree_page_leaf;
class abstract_tree_page_branch;
class abstract_tree_elem;
class abstract_tx_aware_tree_elem;
class abstract_tree_page_branch_elem;
class abstract_tree_page_branch_key;

template<typename Key, typename Val, typename... Augments>
class tree_impl;

template<typename Key, typename Val, typename... Augments>
class tree_page_leaf;

template<typename Key, typename Val, typename... Augments>
class tree_page_branch;

template<typename Key, typename Val, typename... Augments>
class tree_elem;


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
  ~abstract_tree() noexcept override = 0;

  public:
  const std::shared_ptr<const tree_cfg> cfg;
  allocator_type allocator;

  ///\brief Compute augmentation of a sequence of elements.
  ///\param off The offset to populate the augment with.
  ///\param elems The elements from which to compute an augmentation.
  virtual auto compute_augment_(std::uint64_t off, const std::vector<cycle_ptr::cycle_gptr<const abstract_tree_elem>>& elems) const -> std::shared_ptr<abstract_tree_page_branch_elem> = 0;
  ///\brief Compute augmentation using reduction of augmentations.
  ///\param off The offset to populate the augment with.
  ///\param elems The augmentations from which to compute an augmentation.
  virtual auto compute_augment_(std::uint64_t off, const std::vector<std::shared_ptr<const abstract_tree_page_branch_elem>>& elems) const -> std::shared_ptr<abstract_tree_page_branch_elem> = 0;

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

  ///\brief Decode this page from a file.
  virtual void decode(const txfile::transaction& tx, std::uint64_t off) = 0;
  ///\brief Encode this page to a file.
  virtual void encode(txfile::transaction& tx) const = 0;

  ///\brief Retrieve the offset of this page.
  auto offset() const noexcept -> std::uint64_t { return off_; }
  ///\brief Test if the given mutex belongs to this page.
  auto is_my_mutex(const std::shared_mutex* m) const noexcept -> bool { return m == &mtx_; }

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
  friend abstract_tx_aware_tree_elem;

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

  public:
  void init_empty(std::uint64_t off);
  void decode(const txfile::transaction& tx, std::uint64_t off) override final;
  void encode(txfile::transaction& tx) const override final;

  private:
  ///\brief Allocate a new element.
  virtual auto allocate_elem_(abstract_tree::allocator_type alloc) const -> cycle_ptr::cycle_gptr<abstract_tree_elem> = 0;

  /**
   * \brief Split this page in two halves.
   * \param[in] lck The lock on this page.
   * \param[in,out] f The file used to read and write the pages.
   * \param new_page_off Offset in the file at which to write the new page.
   * \param parent Parent page.
   * \param parent_lck Lock on the parent page.
   * \return A tuple consisting of:
   * - the first page of the new pair
   * - the lock on the first page
   * - the lowest key in the second page
   * - the second page of the new pair
   * - the lock on the second page
   */
  auto local_split_(
      const std::unique_lock<std::shared_mutex>& lck, txfile& tx, std::uint64_t new_page_off,
      cycle_ptr::cycle_gptr<abstract_tree_page_branch> parent, const std::unique_lock<std::shared_mutex>& parent_lck)
  -> std::tuple<
      std::shared_ptr<abstract_tree_page_branch_key>,
      cycle_ptr::cycle_gptr<abstract_tree_page_leaf>,
      std::unique_lock<std::shared_mutex>>;
  ///\brief Select element for split.
  ///\throws std::logic_error if the page doesn't contain at least 2 elements.
  auto split_select_(const std::unique_lock<std::shared_mutex>& lck) -> elems_vector::iterator;
  ///\brief Compute offset of abstract_tree_elem at the given index.
  auto offset_for_idx_(elems_vector::size_type idx) const noexcept -> std::uint64_t;
  ///\brief Compute offset of given abstract_tree_elem.
  auto offset_for_(const abstract_tree_elem& elem) const noexcept -> std::uint64_t;

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

  ///\brief Transactional operation for inserting a sibling.
  class insert_sibling_tx;

  private:
  using elems_vector = std::vector<
      std::shared_ptr<abstract_tree_page_branch_elem>,
      abstract_tree::traits_type::rebind_alloc<
          std::shared_ptr<abstract_tree_page_branch_elem>
      >
  >;

  using keys_vector = std::vector<
      std::shared_ptr<abstract_tree_page_branch_key>,
      abstract_tree::traits_type::rebind_alloc<
          std::shared_ptr<abstract_tree_page_branch_key>
      >
  >;

  protected:
  explicit abstract_tree_page_branch(cycle_ptr::cycle_gptr<abstract_tree> tree);
  explicit abstract_tree_page_branch(cycle_ptr::cycle_gptr<abstract_tree_page_branch> parent);
  ~abstract_tree_page_branch() noexcept override;

  public:
  void decode(const txfile::transaction& tx, std::uint64_t off) override final;
  void encode(txfile::transaction& tx) const override final;

  ///\brief Insert a sibling page.
  auto insert_sibling(
      const std::unique_lock<std::shared_mutex>& lck, txfile::transaction& tx,
      const abstract_tree_page& precede_page, std::shared_ptr<abstract_tree_page_branch_elem> precede_augment,
      [[maybe_unused]] const abstract_tree_page& new_sibling, std::shared_ptr<abstract_tree_page_branch_key> sibling_key, std::shared_ptr<abstract_tree_page_branch_elem> sibling_augment)
  -> insert_sibling_tx;

  private:
  virtual auto allocate_elem_(abstract_tree::allocator_type alloc) const -> std::shared_ptr<abstract_tree_page_branch_elem> = 0;
  virtual auto allocate_key_(abstract_tree::allocator_type alloc) const -> std::shared_ptr<abstract_tree_page_branch_key> = 0;

  elems_vector elems_;
  keys_vector keys_;
};


///\brief Internal type used to perform two-phase commit on sibling insertion.
class monsoon_tx_local_ abstract_tree_page_branch::insert_sibling_tx {
  friend abstract_tree_page_branch;

  public:
  insert_sibling_tx(const insert_sibling_tx&) = delete;
  insert_sibling_tx& operator=(const insert_sibling_tx&) = delete;

  insert_sibling_tx() = default;
  insert_sibling_tx(insert_sibling_tx&&) noexcept = default;
  insert_sibling_tx& operator=(insert_sibling_tx&&) = default;

  void commit() noexcept;

  private:
  abstract_tree_page_branch* self = nullptr;
  elems_vector::iterator elems_pos; ///<\brief Points at the precede_page.
  keys_vector::iterator keys_insert_pos; ///<\brief Points at the insert position for the key.
  std::shared_ptr<abstract_tree_page_branch_elem> elem0;
  std::shared_ptr<abstract_tree_page_branch_elem> elem1;
  std::shared_ptr<abstract_tree_page_branch_key> key;
};


class monsoon_tx_export_ abstract_tree_elem
: protected cycle_ptr::cycle_base
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

  ///\brief Test if this value is never visible in any transactions.
  virtual auto is_never_visible() const noexcept -> bool;

  private:
  ///\brief Helper function, retrieves the mtx.
  virtual auto mtx_ref_() const noexcept -> std::shared_mutex& = 0;
  ///\brief Extract the key.
  virtual auto branch_key_(abstract_tree::allocator_type alloc) const -> std::shared_ptr<abstract_tree_page_branch_key> = 0;

  protected:
  cycle_ptr::cycle_member_ptr<abstract_tree_page_leaf> parent_;
};


class monsoon_tx_export_ abstract_tx_aware_tree_elem
: public tx_aware_data,
  public abstract_tree_elem
{
  protected:
  using abstract_tree_elem::abstract_tree_elem;
  virtual ~abstract_tx_aware_tree_elem() noexcept = 0;

  auto is_never_visible() const noexcept -> bool override final;

  private:
  auto mtx_ref_() const noexcept -> std::shared_mutex& override final;
  auto offset() const -> std::uint64_t override final;
};


///\brief Abstract branch element.
///\details Holds the child-page offset and augmentations.
class monsoon_tx_export_ abstract_tree_page_branch_elem {
  public:
  static constexpr std::size_t offset_size = sizeof(std::uint64_t);

  protected:
  abstract_tree_page_branch_elem() noexcept = default;
  explicit abstract_tree_page_branch_elem(std::uint64_t off) noexcept : off(off) {}
  virtual ~abstract_tree_page_branch_elem() noexcept = 0;

  public:
  virtual void decode(boost::asio::const_buffer buf) = 0;
  virtual void encode(boost::asio::mutable_buffer buf) const = 0;

  ///\brief Offset of the page this branch element points at.
  std::uint64_t off = 0;
};


///\brief Abstract key interface.
class monsoon_tx_export_ abstract_tree_page_branch_key {
  protected:
  abstract_tree_page_branch_key() noexcept = default;
  virtual ~abstract_tree_page_branch_key() noexcept = 0;

  public:
  virtual void decode(boost::asio::const_buffer buf) = 0;
  virtual void encode(boost::asio::mutable_buffer buf) const = 0;
};


template<typename Key, typename Val, typename... Augments>
class monsoon_tx_export_ tree_impl
: public abstract_tree
{
  public:
  using abstract_tree::abstract_tree;
  ~tree_impl() noexcept override = 0;

  auto compute_augment_(std::uint64_t off, const std::vector<cycle_ptr::cycle_gptr<const abstract_tree_elem>>& elems) const -> std::shared_ptr<abstract_tree_page_branch_elem> override final;
  auto compute_augment_(std::uint64_t off, const std::vector<std::shared_ptr<const abstract_tree_page_branch_elem>>& elems) const -> std::shared_ptr<abstract_tree_page_branch_elem> override final;

  private:
  ///\brief Augment reducer implementation.
  static auto augment_combine_(const std::tuple<Augments...>& x, const std::tuple<Augments...>& y) -> std::tuple<Augments...>;
  ///\brief Augment reducer implementation, the one that does the actual reducing.
  template<std::size_t... Idxs>
  static auto augment_combine_seq_(const std::tuple<Augments...>& x, const std::tuple<Augments...>& y, [[maybe_unused]] std::index_sequence<Idxs...> seq) -> std::tuple<Augments...>;
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


template<typename Key, typename Val, typename... Augments>
class tree_elem final
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
  -> std::tuple<cycle_ptr::cycle_gptr<tree_page_leaf<Key, Val, Augments...>>, std::shared_lock<std::shared_mutex>>;

  ///\brief Acquire the parent and lock it for write.
  ///\details May only be called without a lock on this element.
  ///\note While the parent is locked, this element can't change parent.
  auto lock_parent_for_write() const
  -> std::tuple<cycle_ptr::cycle_gptr<tree_page_leaf<Key, Val, Augments...>>, std::unique_lock<std::shared_mutex>>;

  private:
  auto branch_key_(abstract_tree::allocator_type alloc) const -> std::shared_ptr<abstract_tree_page_branch_key> override;

  Key key_;
  Val val_;
};


template<typename... Augments>
class tree_page_branch_elem
: public abstract_tree_page_branch_elem
{
  public:
  tree_page_branch_elem()
      noexcept(std::is_nothrow_default_constructible_v<std::tuple<Augments...>>) = default;
  explicit tree_page_branch_elem(std::uint64_t off, std::tuple<Augments...> augments)
      noexcept(std::is_nothrow_move_constructible_v<std::tuple<Augments...>>);

  void decode(boost::asio::const_buffer buf) override final;
  void encode(boost::asio::mutable_buffer buf) const override final;

  private:
  template<std::size_t Idx0, std::size_t... Idxs>
  void decode_idx_seq_(boost::asio::const_buffer buf, [[maybe_unused]] std::index_sequence<Idx0, Idxs...> seq);
  template<std::size_t Idx0, std::size_t... Idxs>
  void encode_idx_seq_(boost::asio::mutable_buffer buf, [[maybe_unused]] std::index_sequence<Idx0, Idxs...> seq) const;

  void decode_idx_seq_(boost::asio::const_buffer buf, [[maybe_unused]] std::index_sequence<> seq) noexcept;
  void encode_idx_seq_(boost::asio::mutable_buffer buf, [[maybe_unused]] std::index_sequence<> seq) const noexcept;

  template<std::size_t Idx>
  void decode_idx_(boost::asio::const_buffer buf);
  template<std::size_t Idx>
  void encode_idx_(boost::asio::mutable_buffer buf) const;

  static auto augment_offset(std::size_t idx) -> std::size_t;

  public:
  std::tuple<Augments...> augments;
};


template<typename Key>
class tree_page_branch_key
: public abstract_tree_page_branch_key
{
  public:
  tree_page_branch_key()
      noexcept(std::is_nothrow_default_constructible_v<Key>) = default;
  explicit tree_page_branch_key(Key&& key)
      noexcept(std::is_nothrow_move_constructible_v<Key>);
  explicit tree_page_branch_key(const Key& key)
      noexcept(std::is_nothrow_copy_constructible_v<Key>);

  void decode(boost::asio::const_buffer buf) override final;
  void encode(boost::asio::mutable_buffer buf) const override final;

  Key key;
};


extern template class monsoon_tx_export_ tree_page_branch_elem<>;


} /* namespace monsoon::tx::detail */

#include "tree_page-inl.h"

#endif /* MONSOON_TX_DETAIL_TREE_PAGE_H */
