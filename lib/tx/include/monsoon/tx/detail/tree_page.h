#ifndef MONSOON_TX_DETAIL_TREE_PAGE_H
#define MONSOON_TX_DETAIL_TREE_PAGE_H

#include <monsoon/tx/txfile.h>
#include <monsoon/tx/tx_aware_data.h>
#include <monsoon/tx/detail/tree_cfg.h>
#include <monsoon/tx/detail/export_.h>
#include <monsoon/shared_resource_allocator.h>
#include <cstddef>
#include <cstdint>
#include <iterator>
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
class tree_page_leaf;
class tree_page_branch;
class abstract_tree_elem;
class abstract_tx_aware_tree_elem;
class abstract_tree_page_branch_elem;
class abstract_tree_page_branch_key;
class abstract_tree_iterator;

template<typename Key, typename Val, typename... Augments>
class tree_impl;

template<typename Key, typename Val, typename... Augments>
class tree_elem;


///\brief Interface and shared logic for a B+ tree.
class monsoon_tx_export_ abstract_tree
: public cycle_ptr::cycle_base
{
  friend abstract_tree_page;
  friend tree_page_leaf;
  friend tree_page_branch;
  friend abstract_tree_elem;
  friend abstract_tree_iterator;

  public:
  ///\brief Allocator used by commit_manager.
  using allocator_type = shared_resource_allocator<std::byte>;
  ///\brief Allocator traits.
  using traits_type = std::allocator_traits<allocator_type>;

  private:
  struct page_with_lock;

  public:
  explicit abstract_tree(allocator_type alloc = allocator_type());
  virtual ~abstract_tree() noexcept = 0;

  private:
  ///\brief Compute augmentation of a sequence of elements.
  ///\param off The offset to populate the augment with.
  ///\param elems The elements from which to compute an augmentation.
  virtual auto compute_augment_(std::uint64_t off, const std::vector<cycle_ptr::cycle_gptr<const abstract_tree_elem>>& elems) const -> std::shared_ptr<abstract_tree_page_branch_elem> = 0;
  ///\brief Compute augmentation using reduction of augmentations.
  ///\param off The offset to populate the augment with.
  ///\param elems The augmentations from which to compute an augmentation.
  virtual auto compute_augment_(std::uint64_t off, const std::vector<std::shared_ptr<const abstract_tree_page_branch_elem>>& elems) const -> std::shared_ptr<abstract_tree_page_branch_elem> = 0;

  ///\brief Retrieve the page only if it is already loaded in memory.
  ///\param off Offset of the page.
  ///\returns Decoded page.
  virtual auto get_if_present(std::uint64_t off) const noexcept -> cycle_ptr::cycle_gptr<abstract_tree_page> = 0;
  ///\brief Retrieve the page.
  ///\param off Offset of the page.
  ///\returns Decoded page.
  virtual auto get(std::uint64_t off) const -> cycle_ptr::cycle_gptr<abstract_tree_page> = 0;
  ///\brief Invalidate the page.
  ///\details Future invocations of get and get_if_present won't return the page anymore.
  ///\param off Offset of the page to invalidate.
  virtual void invalidate(std::uint64_t off) const noexcept = 0;

  ///\brief Default-allocate an tree_page_leaf.
  auto allocate_leaf_() -> cycle_ptr::cycle_gptr<tree_page_leaf>;
  ///\brief Default-allocate an tree_page_branch.
  auto allocate_branch_() -> cycle_ptr::cycle_gptr<tree_page_branch>;
  ///\brief Allocate an element.
  virtual auto allocate_elem_(cycle_ptr::cycle_gptr<tree_page_leaf> parent) const -> cycle_ptr::cycle_gptr<abstract_tree_elem> = 0;

  ///\brief Allocate a branch element.
  virtual auto allocate_branch_elem_() const -> std::shared_ptr<abstract_tree_page_branch_elem> = 0;
  ///\brief Allocate a branch key.
  virtual auto allocate_branch_key_() const -> std::shared_ptr<abstract_tree_page_branch_key> = 0;

  ///\brief Get the first element in the tree.
  auto first_element_() -> cycle_ptr::cycle_gptr<abstract_tree_elem>;
  ///\brief Get the last element in the tree.
  auto last_element_() -> cycle_ptr::cycle_gptr<abstract_tree_elem>;

  ///\brief Invoke less-than comparison between two keys.
  virtual auto less_cb(const abstract_tree_page_branch_key&  x, const abstract_tree_page_branch_key&  y) const -> bool = 0;
  ///\brief Invoke less-than comparison between two elements.
  virtual auto less_cb(const abstract_tree_elem&             x, const abstract_tree_elem&             y) const -> bool;
  ///\brief Invoke less-than comparison between a key and an element.
  virtual auto less_cb(const abstract_tree_page_branch_key&  x, const abstract_tree_elem&             y) const -> bool;
  ///\brief Invoke less-than comparison between an element and a key.
  virtual auto less_cb(const abstract_tree_elem&             x, const abstract_tree_page_branch_key&  y) const -> bool;

  protected:
  auto begin() -> abstract_tree_iterator;
  auto end() -> abstract_tree_iterator;

  /**
   * \brief Find the iterators describing a range where all elements use the value "key", and then invoke a callback.
   * \details
   * Find the lower and upper bounds for \p key and then invoke \p cb on those.
   * While \p cb is executed, pages used in the equal range will be locked for read.
   *
   * \note
   * The reason for having a callback-based approach, is because the tree can be accessed and modified concurrently.
   * This callback approach ensures that the appropriate locks are held to prevent this.
   *
   * \param cb Callback to invoke. This takes the lower-bound and upper-bound iterators as arguments.
   * \param key The key to find the equal range for.
   */
  void with_equal_range_for_read(cheap_fn_ref<void(abstract_tree_iterator, abstract_tree_iterator)> cb, const abstract_tree_page_branch_key& key);
  /**
   * \brief Find the iterators describing a range where all elements use the value "key", and then invoke a callback.
   * \details
   * Find the lower and upper bounds for \p key and then invoke \p cb on those.
   * While \p cb is executed, pages used in the equal range will be locked for write.
   *
   * \note
   * The reason for having a callback-based approach, is because the tree can be accessed and modified concurrently.
   * This callback approach ensures that the appropriate locks are held to prevent this.
   *
   * \param cb Callback to invoke. This takes the lower-bound and upper-bound iterators as arguments.
   * \param key The key to find the equal range for.
   */
  void with_equal_range_for_write(cheap_fn_ref<void(abstract_tree_iterator, abstract_tree_iterator)> cb, const abstract_tree_page_branch_key& key);

  private:
  /**
   * \brief Find the iterators describing a range where all elements use the value "key", and then invoke a callback.
   * \details
   * Find the lower and upper bounds for \p key and then invoke \p cb on those.
   * While \p cb is executed, pages used in the equal range will be locked.
   *
   * This template powers both the with_equal_range_read and with_equal_range_for_write methods.
   *
   * \note
   * The reason for having a callback-based approach, is because the tree can be accessed and modified concurrently.
   * This callback approach ensures that the appropriate locks are held to prevent this.
   *
   * \tparam LockType A std::shared_lock<std::shared_mutex> or std::unique_lock<std::mutex>, used to maintain the lock.
   * \param cb Callback to invoke. This takes the lower-bound and upper-bound iterators as arguments.
   * \param key The key to find the equal range for.
   */
  template<typename LockType>
  void with_equal_range_(cheap_fn_ref<void(abstract_tree_iterator, abstract_tree_iterator)> cb, const abstract_tree_page_branch_key& key);

  public:
  const std::shared_ptr<const tree_cfg> cfg;
  allocator_type allocator;

  protected:
  std::uint64_t root_off_ = 0;
  mutable std::shared_mutex mtx_;
};


///\brief Shared base for B+ tree pages.
class monsoon_tx_export_ abstract_tree_page
: protected cycle_ptr::cycle_base
{
  friend abstract_tree;

  public:
  explicit abstract_tree_page(cycle_ptr::cycle_gptr<abstract_tree> tree);
  virtual ~abstract_tree_page() noexcept = 0;

  ///\brief Get the tree owning this page.
  ///\returns Pointer to the tree.
  ///\throws std::bad_weak_ptr If the tree has gone away.
  auto tree() const -> cycle_ptr::cycle_gptr<abstract_tree>;

  ///\brief Decode a page.
  ///\details Uses the magic encoded in \p tx to determine what type of page is stored at the offset.
  ///\param tree Tree that this page belongs to.
  ///\param[in] tx Source of bytes on which the decode operation operates.
  ///\param off Offset in \p tx where the page is located.
  ///\return A newly allocated page, that was read from bytes in \p tx at offset \p off and is of the appropriate type.
  static auto decode(
      abstract_tree& tree,
      const txfile::transaction& tx, std::uint64_t off)
  -> cycle_ptr::cycle_gptr<abstract_tree_page>;

  ///\brief Decode this page from a file.
  virtual void decode(const txfile::transaction& tx, std::uint64_t off) = 0;
  ///\brief Encode this page to a file.
  virtual void encode(txfile::transaction& tx) const = 0;

  ///\brief Retrieve the offset of this page.
  auto offset() const noexcept -> std::uint64_t { return off_; }
  ///\brief Test if the given mutex belongs to this page.
  auto is_my_mutex(const std::shared_mutex* m) const noexcept -> bool { return m == &mtx_; }
  ///\brief Update parent offset, after reparenting.
  ///\note On-disk representation should already be updated.
  void reparent_([[maybe_unused]] std::uint64_t old_parent_off, std::uint64_t new_parent_off) noexcept;

  private:
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
      cycle_ptr::cycle_gptr<tree_page_branch> parent, const std::unique_lock<std::shared_mutex>& parent_lck)
  -> std::tuple<
      std::shared_ptr<abstract_tree_page_branch_key>,
      cycle_ptr::cycle_gptr<abstract_tree_page>,
      std::unique_lock<std::shared_mutex>>;
  ///\brief local_split_ virtual function.
  ///\note This is a virtual function so that we can "fake" covariant return using the local_split_ functions.
  virtual auto local_split_atp_(
      const std::unique_lock<std::shared_mutex>& lck, txfile& tx, std::uint64_t new_page_off,
      cycle_ptr::cycle_gptr<tree_page_branch> parent, const std::unique_lock<std::shared_mutex>& parent_lck)
  -> std::tuple<
      std::shared_ptr<abstract_tree_page_branch_key>,
      cycle_ptr::cycle_gptr<abstract_tree_page>,
      std::unique_lock<std::shared_mutex>> = 0;

  protected:
  std::uint64_t off_ = 0, parent_off_ = 0;
  mutable std::shared_mutex mtx_;

  private:
  const cycle_ptr::cycle_weak_ptr<abstract_tree> tree_;

  public:
  const std::shared_ptr<const tree_cfg> cfg;
};


///\brief Leaf page of a B+ tree.
class monsoon_tx_export_ tree_page_leaf final
: public abstract_tree_page
{
  friend abstract_tree;
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

  public:
  explicit tree_page_leaf(cycle_ptr::cycle_gptr<abstract_tree> tree);
  ~tree_page_leaf() noexcept override;

  void init_empty(std::uint64_t off);
  void decode(const txfile::transaction& tx, std::uint64_t off) override final;
  void encode(txfile::transaction& tx) const override final;

  ///\brief Retrieve the next leaf page.
  auto next() const -> cycle_ptr::cycle_gptr<tree_page_leaf>;
  ///\brief Retrieve the previous leaf page.
  auto prev() const -> cycle_ptr::cycle_gptr<tree_page_leaf>;
  ///\brief Retrieve the next leaf page.
  ///\param lck A lock on this page.
  auto next(const std::shared_lock<std::shared_mutex>& lck) const -> cycle_ptr::cycle_gptr<tree_page_leaf>;
  ///\brief Retrieve the previous leaf page.
  ///\param lck A lock on this page.
  auto prev(const std::shared_lock<std::shared_mutex>& lck) const -> cycle_ptr::cycle_gptr<tree_page_leaf>;

  private:
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
      cycle_ptr::cycle_gptr<tree_page_branch> parent, const std::unique_lock<std::shared_mutex>& parent_lck)
  -> std::tuple<
      std::shared_ptr<abstract_tree_page_branch_key>,
      cycle_ptr::cycle_gptr<tree_page_leaf>,
      std::unique_lock<std::shared_mutex>>;
  auto local_split_atp_(
      const std::unique_lock<std::shared_mutex>& lck, txfile& tx, std::uint64_t new_page_off,
      cycle_ptr::cycle_gptr<tree_page_branch> parent, const std::unique_lock<std::shared_mutex>& parent_lck)
  -> std::tuple<
      std::shared_ptr<abstract_tree_page_branch_key>,
      cycle_ptr::cycle_gptr<abstract_tree_page>,
      std::unique_lock<std::shared_mutex>> override final;
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


///\brief Branch page of a B+ tree.
class monsoon_tx_export_ tree_page_branch final
: public abstract_tree_page
{
  friend abstract_tree;

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

  public:
  explicit tree_page_branch(cycle_ptr::cycle_gptr<abstract_tree> tree);
  ~tree_page_branch() noexcept override;

  void decode(const txfile::transaction& tx, std::uint64_t off) override final;
  void encode(txfile::transaction& tx) const override final;

  ///\brief Insert a sibling page.
  auto insert_sibling(
      const std::unique_lock<std::shared_mutex>& lck, txfile::transaction& tx,
      const abstract_tree_page& precede_page, std::shared_ptr<abstract_tree_page_branch_elem> precede_augment,
      [[maybe_unused]] const abstract_tree_page& new_sibling, std::shared_ptr<abstract_tree_page_branch_key> sibling_key, std::shared_ptr<abstract_tree_page_branch_elem> sibling_augment)
  -> insert_sibling_tx;

  private:
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
      cycle_ptr::cycle_gptr<tree_page_branch> parent, const std::unique_lock<std::shared_mutex>& parent_lck)
  -> std::tuple<
      std::shared_ptr<abstract_tree_page_branch_key>,
      cycle_ptr::cycle_gptr<tree_page_branch>,
      std::unique_lock<std::shared_mutex>>;
  auto local_split_atp_(
      const std::unique_lock<std::shared_mutex>& lck, txfile& tx, std::uint64_t new_page_off,
      cycle_ptr::cycle_gptr<tree_page_branch> parent, const std::unique_lock<std::shared_mutex>& parent_lck)
  -> std::tuple<
      std::shared_ptr<abstract_tree_page_branch_key>,
      cycle_ptr::cycle_gptr<abstract_tree_page>,
      std::unique_lock<std::shared_mutex>> override final;

  elems_vector elems_;
  keys_vector keys_;
};


///\brief Internal type used to perform two-phase commit on sibling insertion.
class monsoon_tx_local_ tree_page_branch::insert_sibling_tx {
  friend tree_page_branch;

  public:
  insert_sibling_tx(const insert_sibling_tx&) = delete;
  insert_sibling_tx& operator=(const insert_sibling_tx&) = delete;

  insert_sibling_tx() = default;
  insert_sibling_tx(insert_sibling_tx&&) noexcept = default;
  insert_sibling_tx& operator=(insert_sibling_tx&&) = default;

  void commit() noexcept;

  private:
  tree_page_branch* self = nullptr;
  elems_vector::iterator elems_pos; ///<\brief Points at the precede_page.
  keys_vector::iterator keys_insert_pos; ///<\brief Points at the insert position for the key.
  std::shared_ptr<abstract_tree_page_branch_elem> elem0;
  std::shared_ptr<abstract_tree_page_branch_elem> elem1;
  std::shared_ptr<abstract_tree_page_branch_key> key;
};


///\brief Base implementation of an element in a B+ tree.
class monsoon_tx_export_ abstract_tree_elem
: protected cycle_ptr::cycle_base
{
  friend abstract_tree;
  friend tree_page_leaf;

  public:
  explicit abstract_tree_elem(cycle_ptr::cycle_gptr<tree_page_leaf> parent);
  virtual ~abstract_tree_elem() noexcept = 0;

  virtual void decode(boost::asio::const_buffer buf) = 0;
  virtual void encode(boost::asio::mutable_buffer buf) const = 0;

  ///\brief Acquire the parent and lock it for read.
  ///\details May only be called without a lock on this element.
  ///\note While the parent is locked, this element can't change parent.
  auto lock_parent_for_read() const
  -> std::tuple<cycle_ptr::cycle_gptr<tree_page_leaf>, std::shared_lock<std::shared_mutex>>;

  ///\brief Acquire the parent and lock it for read.
  ///\details May only be called without a lock on this element.
  ///\note While the parent is locked, this element can't change parent.
  auto lock_parent_for_read(std::shared_lock<std::shared_mutex>& self_lck) const
  -> std::tuple<cycle_ptr::cycle_gptr<tree_page_leaf>, std::shared_lock<std::shared_mutex>>;

  ///\brief Acquire the parent and lock it for write.
  ///\details May only be called without a lock on this element.
  ///\note While the parent is locked, this element can't change parent.
  auto lock_parent_for_write() const
  -> std::tuple<cycle_ptr::cycle_gptr<tree_page_leaf>, std::unique_lock<std::shared_mutex>>;

  ///\brief Acquire the parent and lock it for write.
  ///\details May only be called without a lock on this element.
  ///\note While the parent is locked, this element can't change parent.
  auto lock_parent_for_write(std::shared_lock<std::shared_mutex>& self_lck) const
  -> std::tuple<cycle_ptr::cycle_gptr<tree_page_leaf>, std::unique_lock<std::shared_mutex>>;

  ///\brief Test if this value is never visible in any transactions.
  virtual auto is_never_visible() const noexcept -> bool;

  ///\brief Retrieve the successor.
  auto next() const -> cycle_ptr::cycle_gptr<abstract_tree_elem>;
  ///\brief Retrieve the predecessor.
  auto prev() const -> cycle_ptr::cycle_gptr<abstract_tree_elem>;

  private:
  ///\brief Helper function, retrieves the mtx.
  virtual auto mtx_ref_() const noexcept -> std::shared_mutex& = 0;
  ///\brief Extract the key.
  virtual auto branch_key_(abstract_tree::allocator_type alloc) const -> std::shared_ptr<abstract_tree_page_branch_key> = 0;

  protected:
  cycle_ptr::cycle_member_ptr<tree_page_leaf> parent_;

  private:
  cycle_ptr::cycle_member_ptr<abstract_tree_elem> pred_, succ_;
};


///\brief Base implementation of an element in a B+ tree, that is transaction aware.
class monsoon_tx_export_ abstract_tx_aware_tree_elem
: public tx_aware_data,
  public abstract_tree_elem
{
  public:
  using abstract_tree_elem::abstract_tree_elem;
  virtual ~abstract_tx_aware_tree_elem() noexcept = 0;

  protected:
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

  abstract_tree_page_branch_elem() noexcept = default;
  explicit abstract_tree_page_branch_elem(std::uint64_t off) noexcept : off(off) {}
  virtual ~abstract_tree_page_branch_elem() noexcept = 0;

  virtual void decode(boost::asio::const_buffer buf) = 0;
  virtual void encode(boost::asio::mutable_buffer buf) const = 0;

  ///\brief Offset of the page this branch element points at.
  std::uint64_t off = 0;
};


///\brief Abstract key interface.
class monsoon_tx_export_ abstract_tree_page_branch_key {
  public:
  abstract_tree_page_branch_key() noexcept = default;
  virtual ~abstract_tree_page_branch_key() noexcept = 0;

  virtual void decode(boost::asio::const_buffer buf) = 0;
  virtual void encode(boost::asio::mutable_buffer buf) const = 0;
};


///\brief Bidirectional iterator over the element of the tree.
class abstract_tree_iterator {
  friend abstract_tree;

  public:
  using iterator_category = std::bidirectional_iterator_tag;
  using value_type = abstract_tree_elem;
  using reference = value_type&;
  using pointer = cycle_ptr::cycle_gptr<value_type>;
  using difference_type = std::int64_t;

  constexpr abstract_tree_iterator() noexcept = default;
  abstract_tree_iterator(const abstract_tree_iterator&) noexcept = default;
  abstract_tree_iterator(abstract_tree_iterator&&) noexcept = default;
  abstract_tree_iterator& operator=(const abstract_tree_iterator&) noexcept = default;
  abstract_tree_iterator& operator=(abstract_tree_iterator&&) noexcept = default;

  private:
  abstract_tree_iterator(cycle_ptr::cycle_gptr<abstract_tree> tree, cycle_ptr::cycle_gptr<abstract_tree_elem> elem) noexcept;

  public:
  auto get() const noexcept -> pointer;
  auto operator*() const noexcept -> reference;
  auto operator->() const noexcept -> const pointer&;

  auto operator++() -> abstract_tree_iterator&;
  auto operator--() -> abstract_tree_iterator&;
  auto operator++(int) -> abstract_tree_iterator;
  auto operator--(int) -> abstract_tree_iterator;

  auto operator==(const abstract_tree_iterator& y) const noexcept -> bool;
  auto operator!=(const abstract_tree_iterator& y) const noexcept -> bool;

  private:
  cycle_ptr::cycle_gptr<abstract_tree> tree_;
  cycle_ptr::cycle_gptr<abstract_tree_elem> elem_;
};


///\brief Tree implementation.
///\note This does not handle the comparison logic, which is delegated instead.
template<typename Key, typename Val, typename... Augments>
class monsoon_tx_export_ tree_impl
: public abstract_tree
{
  public:
  using abstract_tree::abstract_tree;
  ~tree_impl() noexcept override = 0;

  private:
  auto compute_augment_(std::uint64_t off, const std::vector<cycle_ptr::cycle_gptr<const abstract_tree_elem>>& elems) const -> std::shared_ptr<abstract_tree_page_branch_elem> override final;
  auto compute_augment_(std::uint64_t off, const std::vector<std::shared_ptr<const abstract_tree_page_branch_elem>>& elems) const -> std::shared_ptr<abstract_tree_page_branch_elem> override final;

  ///\brief Augment reducer implementation.
  static auto augment_combine_(const std::tuple<Augments...>& x, const std::tuple<Augments...>& y) -> std::tuple<Augments...>;
  ///\brief Augment reducer implementation, the one that does the actual reducing.
  template<std::size_t... Idxs>
  static auto augment_combine_seq_(const std::tuple<Augments...>& x, const std::tuple<Augments...>& y, [[maybe_unused]] std::index_sequence<Idxs...> seq) -> std::tuple<Augments...>;

  auto allocate_elem_(cycle_ptr::cycle_gptr<tree_page_leaf> parent) const -> cycle_ptr::cycle_gptr<abstract_tree_elem> override final;
  auto allocate_branch_elem_() const -> std::shared_ptr<abstract_tree_page_branch_elem> override final;
  auto allocate_branch_key_() const -> std::shared_ptr<abstract_tree_page_branch_key> override final;
};


///\brief Transaction aware key-value element in a B+ tree.
template<typename Key, typename Val, typename... Augments>
class tree_elem final
: public abstract_tree_elem
{
  public:
  static constexpr std::size_t SIZE = tx_aware_data::TX_AWARE_SIZE + Key::SIZE + Val::SIZE;

  tree_elem(cycle_ptr::cycle_gptr<tree_page_leaf> parent, const Key& key, const Val& val);
  tree_elem(cycle_ptr::cycle_gptr<tree_page_leaf> parent, Key&& key, Val&& val);
  explicit tree_elem(cycle_ptr::cycle_gptr<tree_page_leaf> parent);

  void decode(boost::asio::const_buffer buf) override;
  void encode(boost::asio::mutable_buffer buf) const override;

  private:
  auto branch_key_(abstract_tree::allocator_type alloc) const -> std::shared_ptr<abstract_tree_page_branch_key> override;

  Key key_;
  Val val_;
};


///\brief Implementation of abstract_tree_page_branch_elem.
template<typename... Augments>
class tree_page_branch_elem final
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


///\brief Implementation of abstract_tree_page_branch_key.
template<typename Key>
class tree_page_branch_key final
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
