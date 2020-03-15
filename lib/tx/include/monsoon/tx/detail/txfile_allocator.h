#ifndef MONSOON_TX_DETAIL_TXFILE_ALLOCATOR_H
#define MONSOON_TX_DETAIL_TXFILE_ALLOCATOR_H

#include <monsoon/tx/detail/export_.h>
#include <monsoon/tx/detail/tree_page.h>
#include <monsoon/tx/db.h>
#include <cstdint>
#include <functional>
#include <shared_mutex>
#include <tuple>
#include <unordered_map>
#include <boost/asio/buffer.hpp>
#include <cycle_ptr/cycle_ptr.h>
#include <cycle_ptr/allocator.h>

namespace monsoon::tx::detail {


class monsoon_tx_export_ txfile_allocator final
: public abstract_tree,
  public db::db_obj
{
  private:
  ///\brief The tree is keyed on address.
  class monsoon_tx_local_ key {
    public:
    static constexpr std::size_t SIZE = 8;

    key() noexcept = default;

    void decode(boost::asio::const_buffer buf);
    void encode(boost::asio::mutable_buffer buf) const;

    auto operator==(const key& y) const noexcept -> bool;
    auto operator!=(const key& y) const noexcept -> bool;

    std::uint64_t addr;
  };

  ///\brief Elements of the allocator.
  class monsoon_tx_local_ element final
  : public abstract_tree_elem
  {
    public:
    static constexpr std::size_t SIZE = key::SIZE + 16;

    using abstract_tree_elem::abstract_tree_elem;
    ~element() noexcept override;

    void decode(boost::asio::const_buffer buf) override;
    void encode(boost::asio::mutable_buffer buf) const override;

    protected:
    auto is_never_visible() const noexcept -> bool override;

    private:
    auto mtx_ref_() const noexcept -> std::shared_mutex& override;
    auto branch_key_(allocator_type alloc) const -> std::shared_ptr<abstract_tree_page_branch_key> override;

    mutable std::shared_mutex mtx_;

    public:
    class key key;
    std::uint64_t used = 0, free = 0;
  };

  ///\brief Augment that keeps track of the largest bit of free space in a sub-tree.
  class monsoon_tx_local_ max_free_space_augment {
    public:
    static constexpr std::size_t SIZE = 8;

    max_free_space_augment() noexcept {}
    max_free_space_augment(const element& e) noexcept : free(e.free) {}

    void decode(boost::asio::const_buffer buf);
    void encode(boost::asio::mutable_buffer buf) const;
    static auto merge(const max_free_space_augment& x, const max_free_space_augment& y) noexcept -> const max_free_space_augment&;

    std::uint64_t free = 0;
  };

  using branch_elem = tree_page_branch_elem<max_free_space_augment>;

  public:
  txfile_allocator() = delete;
  txfile_allocator(const txfile& f, std::uint64_t off, allocator_type alloc = allocator_type());
  ~txfile_allocator() noexcept override;

  private:
  auto compute_augment_(std::uint64_t off, const std::vector<cycle_ptr::cycle_gptr<const abstract_tree_elem>>& elems) const -> std::shared_ptr<abstract_tree_page_branch_elem> override;
  auto compute_augment_(std::uint64_t off, const std::vector<std::shared_ptr<const abstract_tree_page_branch_elem>>& elems) const -> std::shared_ptr<abstract_tree_page_branch_elem> override;

  auto get_if_present(std::uint64_t off) const noexcept -> cycle_ptr::cycle_gptr<abstract_tree_page> override;
  auto get(std::uint64_t off) const -> cycle_ptr::cycle_gptr<abstract_tree_page> override;
  void invalidate(std::uint64_t off) const noexcept override;

  auto allocate_elem_(cycle_ptr::cycle_gptr<tree_page_leaf> parent) const -> cycle_ptr::cycle_gptr<abstract_tree_elem> override;
  auto allocate_branch_elem_() const -> std::shared_ptr<abstract_tree_page_branch_elem> override;
  auto allocate_branch_key_() const -> std::shared_ptr<abstract_tree_page_branch_key> override;

  ///\brief Implementation of the merge logic.
  static auto augment_merge_(const std::tuple<max_free_space_augment>& x, const std::tuple<max_free_space_augment>& y) -> std::tuple<max_free_space_augment>;

  // XXX this will need to be replaced with a cache at some point, provided by the database.
  using page_map = std::unordered_map<
      std::uint64_t,
      cycle_ptr::cycle_member_ptr<abstract_tree_page>,
      std::hash<std::uint64_t>,
      std::equal_to<std::uint64_t>,
      cycle_ptr::cycle_allocator<
          traits_type::rebind_alloc<std::pair<const std::uint64_t, cycle_ptr::cycle_member_ptr<abstract_tree_page>>>
      >
  >;
  mutable page_map pages_{ page_map::allocator_type(*this, allocator) }; // XXX replace with database cache
  mutable std::shared_mutex pages_mtx_; // XXX remove
  txfile f_; // XXX replace with database
};


} /* namespace monsoon::tx::detail */

#include "txfile_allocator-inl.h"

#endif /* MONSOON_TX_DETAIL_TXFILE_ALLOCATOR_H */
