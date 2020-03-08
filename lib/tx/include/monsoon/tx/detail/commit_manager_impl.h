#ifndef MONSOON_TX_DETAIL_COMMIT_MANAGER_IMPL_H
#define MONSOON_TX_DETAIL_COMMIT_MANAGER_IMPL_H

#include <monsoon/tx/detail/export_.h>
#include <monsoon/tx/detail/commit_manager.h>
#include <monsoon/tx/txfile.h>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <system_error>
#include <variant>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/options.hpp>
#include <boost/intrusive/link_mode.hpp>

namespace monsoon::tx::detail {


class monsoon_tx_export_ commit_manager_impl
: public commit_manager,
  public std::enable_shared_from_this<commit_manager_impl>
{
  public:
  static constexpr std::uint32_t magic = 0x697f'6431;

  private:
  static constexpr type max_tx_delta = std::numeric_limits<type>::max();
  static constexpr type prealloc_batch = type(1) << (std::numeric_limits<type>::digits / 2u);

  static constexpr std::size_t OFF_MAGIC = 0;
  static constexpr std::size_t OFF_TX_START = OFF_MAGIC + sizeof(magic);
  static constexpr std::size_t OFF_LAST_WRITE_COMMIT_ID = OFF_TX_START + sizeof(type);
  static constexpr std::size_t OFF_COMPLETED_COMMIT_ID = OFF_LAST_WRITE_COMMIT_ID + sizeof(type);

  public:
  static constexpr std::size_t SIZE = OFF_COMPLETED_COMMIT_ID + sizeof(type);
  static_assert(SIZE <= commit_manager::SIZE);

  private:
  ///\brief Implement commit ID state.
  class monsoon_tx_local_ state_impl_ final
  : public state_,
    public std::enable_shared_from_this<state_impl_>,
    public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>>
  {
    public:
    state_impl_(type tx_start, type val, commit_manager_impl& cm)
    : state_(tx_start, val),
      cm(cm.shared_from_this())
    {}

    ~state_impl_() noexcept;

    auto get_cm_or_null() const noexcept -> std::shared_ptr<commit_manager> override;

    private:
    std::weak_ptr<commit_manager_impl> cm;
  };

  ///\brief Shared state for a write operation.
  class monsoon_tx_local_ write_id_state_impl_ final
  : public write_id_state_,
    public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>>
  {
    friend commit_manager_impl;

    public:
    write_id_state_impl_(commit_id seq, txfile::transaction&& tx) noexcept;
    ~write_id_state_impl_() noexcept;

    private:
    ///\brief Waits until this transaction is at the front of the transaction queue.
    void wait_until_front_transaction_(const commit_manager_impl& cm);
    auto do_apply(cheap_fn_ref<std::error_code> validation, cheap_fn_ref<> phase2) -> std::error_code override;

    std::variant<std::monostate, std::condition_variable_any> wait_;
  };

  using state_list = boost::intrusive::list<
      state_impl_,
      boost::intrusive::constant_time_size<false>>;

  using write_list = boost::intrusive::list<
      write_id_state_impl_,
      boost::intrusive::constant_time_size<false>>;

  private:
  commit_manager_impl(monsoon::io::fd::offset_type off, allocator_type alloc);
  ~commit_manager_impl() noexcept override;

  public:
  ///\brief Allocate a commit manager.
  ///\param[in] f Use the file to initialize state.
  ///\param off Offset in \p f at which state is stored.
  ///\param alloc Allocator to use for internal data structures.
  static auto allocate(const txfile& f, monsoon::io::fd::offset_type off, allocator_type alloc = allocator_type()) -> std::shared_ptr<commit_manager_impl>;
  ///\brief Initialize a new commit manager.
  ///\param[in,out] tx Write transaction in the file.
  ///\param off Offset in \p tx at which to initialize the commit manager.
  static void init(txfile::transaction& tx, monsoon::io::fd::offset_type off);

  private:
  auto do_get_tx_commit_id_(allocator_type tx_alloc) const -> commit_id override;
  auto do_prepare_commit_(txfile& f, allocator_type tx_alloc) -> write_id override;

  ///\brief Accept the dropped write state.
  void null_commit_(write_id_state_impl_& s) noexcept;
  ///\brief See if we can start the front commit.
  void maybe_start_front_write_locked_(const std::unique_lock<std::shared_mutex>& lck) noexcept;

  auto suggest_vacuum_target_() const -> commit_id override;
  void on_completed_vacuum_(txfile& f, commit_id vacuum_target) override;

  ///\brief Transaction IDs in the database start at this value.
  type tx_start_;
  ///\brief List of active tx_start values.
  ///\details The front of this list shows the oldest in-use commit ID.
  ///\details The back of this list shows the most-recent in-use commit ID.
  ///\invariant The tx_start of the states will ascend, and be greater than tx_start_.
  state_list states_;
  ///\brief Lock for allocating/mutating collection.
  mutable std::shared_mutex mtx_;
  ///\brief Lock held during commits, or to lock out commits.
  mutable std::shared_mutex commit_mtx_;
  ///\brief Most-recent handed-out ID for a commit.
  type last_write_commit_id_;
  ///\brief Number of last_write_commit_id_ we have pre-allocated.
  type last_write_commit_id_avail_ = 0;
  ///\brief Most-recent completed ID for a commit.
  commit_id completed_commit_id_;
  ///\brief List of pending writes.
  ///\details The front of the list contains the first-to-be-written transaction.
  ///\note A write may not be ready to be written.
  write_list writes_;
  ///\brief Offset in txfile of this.
  monsoon::io::fd::offset_type off_;
};



} /* namespace monsoon::tx::detail */

#include "commit_manager_impl-inl.h"

#endif /* MONSOON_TX_DETAIL_COMMIT_MANAGER_IMPL_H */
