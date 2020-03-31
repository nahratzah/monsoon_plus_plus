#ifndef MONSOON_TX_DETAIL_TXFILE_ALLOCATOR_H
#define MONSOON_TX_DETAIL_TXFILE_ALLOCATOR_H

#include <monsoon/tx/detail/export_.h>
#include <monsoon/tx/detail/tree_page.h>
#include <monsoon/tx/detail/txfile_allocator_log.h>
#include <monsoon/tx/db.h>
#include <monsoon/tx/detail/db_cache.h>
#include <cstdint>
#include <functional>
#include <optional>
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
    explicit key(std::uint64_t addr) : addr(addr) {}

    void decode(boost::asio::const_buffer buf);
    void encode(boost::asio::mutable_buffer buf) const;

    auto operator==(const key& y) const noexcept -> bool;
    auto operator!=(const key& y) const noexcept -> bool;
    auto operator< (const key& y) const noexcept -> bool;
    auto operator> (const key& y) const noexcept -> bool;
    auto operator<=(const key& y) const noexcept -> bool;
    auto operator>=(const key& y) const noexcept -> bool;

    std::uint64_t addr;
  };

  ///\brief Elements of the allocator.
  class monsoon_tx_local_ element final
  : public abstract_tree_elem
  {
    friend txfile_allocator;

    public:
    static constexpr std::size_t SIZE = key::SIZE + 16;

    using abstract_tree_elem::abstract_tree_elem;
    element(cycle_ptr::cycle_gptr<tree_page_leaf> parent,
        const class key& key, std::uint64_t used = 0, std::uint64_t free = 0);
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
  txfile_allocator(std::shared_ptr<monsoon::tx::db> db, std::uint64_t off);
  ~txfile_allocator() noexcept override;

  private:
  auto compute_augment_(std::uint64_t off, const std::vector<cycle_ptr::cycle_gptr<const abstract_tree_elem>>& elems, allocator_type allocator) const -> std::shared_ptr<abstract_tree_page_branch_elem> override;
  auto compute_augment_(std::uint64_t off, const std::vector<std::shared_ptr<const abstract_tree_page_branch_elem>>& elems, allocator_type allocator) const -> std::shared_ptr<abstract_tree_page_branch_elem> override;

  auto allocate_elem_(cycle_ptr::cycle_gptr<tree_page_leaf> parent, allocator_type allocator) const -> cycle_ptr::cycle_gptr<abstract_tree_elem> override;
  auto allocate_branch_elem_(allocator_type allocator) const -> std::shared_ptr<abstract_tree_page_branch_elem> override;
  auto allocate_branch_key_(allocator_type allocator) const -> std::shared_ptr<abstract_tree_page_branch_key> override;

  auto less_cb(const abstract_tree_page_branch_key&  x, const abstract_tree_page_branch_key&  y) const -> bool override;
  auto less_cb(const abstract_tree_elem&             x, const abstract_tree_elem&             y) const -> bool override;
  auto less_cb(const abstract_tree_page_branch_key&  x, const abstract_tree_elem&             y) const -> bool override;
  auto less_cb(const abstract_tree_elem&             x, const abstract_tree_page_branch_key&  y) const -> bool override;

  ///\brief Implementation of the merge logic.
  static auto augment_merge_(const std::tuple<max_free_space_augment>& x, const std::tuple<max_free_space_augment>& y) -> std::tuple<max_free_space_augment>;

  auto allocate_txfile_bytes(txfile::transaction& tx, std::uint64_t bytes, allocator_type tx_allocator, tx_op_collection& ops) -> std::uint64_t override;
  /**
   * \brief Allocate bytes by taking it from an entry in the tree.
   *
   * \param tx Transaction in which the allocation takes place.
   * \param bytes Number of bytes to allocate.
   * \param tx_allocator Allocator to use during this transaction.
   * \param ops Commit/rollback handler.
   * \return An address if the allocation succeeds, or an empty optional if the allocation fails.
   */
  monsoon_tx_local_ auto maybe_allocate_txfile_bytes_from_tree_(
      txfile::transaction& tx, std::uint64_t bytes,
      allocator_type tx_allocator, tx_op_collection& ops) -> std::optional<std::uint64_t>;
  /**
   * \brief Stealing allocator.
   * \details
   * This allocator allocates memory by reducing the free-space of elements.
   * Because it doesn't write down the bytes it thus acquires, the space is lost.
   * (Hence the word "stealing": it steals the space from the tree.)
   *
   * \param tx Transaction in which the allocation takes place.
   * \param bytes Number of bytes to allocate.
   * \param tx_allocator Allocator to use during this transaction.
   * \param ops Commit/rollback handler.
   * \return An address if the allocation succeeds, or an empty optional if the allocation fails.
   */
  monsoon_tx_local_ auto steal_allocate_(
      txfile::transaction& tx, std::uint64_t bytes,
      allocator_type tx_allocator,
      tx_op_collection& ops) -> std::optional<std::uint64_t>;

  ///\brief Run tree maintenance.
  ///\details Takes entries from the log and inserts them into the tree.
  monsoon_tx_local_ void do_maintenance_(allocator_type tx_allocator);

  std::shared_ptr<txfile_allocator_log> log_;
};


} /* namespace monsoon::tx::detail */

#include "txfile_allocator-inl.h"

#endif /* MONSOON_TX_DETAIL_TXFILE_ALLOCATOR_H */
