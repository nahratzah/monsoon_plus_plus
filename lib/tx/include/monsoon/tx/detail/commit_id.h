#ifndef MONSOON_TX_DETAIL_COMMIT_ID_H
#define MONSOON_TX_DETAIL_COMMIT_ID_H

#include <monsoon/tx/detail/export_.h>
#include <monsoon/tx/txfile.h>
#include <monsoon/unique_alloc_ptr.h>
#include <monsoon/shared_resource_allocator.h>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <system_error>
#include <variant>
#include <boost/asio/execution_context.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/options.hpp>
#include <boost/intrusive/link_mode.hpp>

namespace monsoon::tx::detail {


class commit_manager;


class monsoon_tx_export_ commit_manager
: public std::enable_shared_from_this<commit_manager>
{
  public:
  using allocator_type = shared_resource_allocator<std::byte>;
  using traits_type = std::allocator_traits<allocator_type>;

  using type = std::uint32_t;
  class write_id;

  private:
  static constexpr std::uint32_t magic = 0x697f'6431;
  static constexpr type max_tx_delta = std::numeric_limits<type>::max();
  static constexpr type prealloc_batch = type(1) << (std::numeric_limits<type>::digits / 2u);

  static constexpr std::size_t OFF_MAGIC = 0;
  static constexpr std::size_t OFF_TX_START = OFF_MAGIC + sizeof(magic);
  static constexpr std::size_t OFF_LAST_WRITE_COMMIT_ID = OFF_TX_START + sizeof(type);
  static constexpr std::size_t OFF_COMPLETED_COMMIT_ID = OFF_LAST_WRITE_COMMIT_ID + sizeof(type);

  public:
  static constexpr std::size_t SIZE = OFF_COMPLETED_COMMIT_ID + sizeof(type);

  private:
  class state_
  : public boost::intrusive::list_base_hook<>
  {
    public:
    state_(type tx_start, commit_manager& cm)
    : tx_start(tx_start),
      cm(cm.shared_from_this())
    {}

    ~state_() noexcept;

    const type tx_start;
    std::weak_ptr<commit_manager> cm;
  };

  public:
  class monsoon_tx_export_ commit_id {
    friend commit_manager;

    public:
    using type = commit_manager::type;

    commit_id() noexcept = default;

    private:
    commit_id(type val, const std::shared_ptr<state_>& s) noexcept;

    public:
    auto tx_start() const noexcept -> type { return s_->tx_start; }
    auto val() const noexcept -> type { return val_; }
    auto relative_val() const noexcept -> type { return val() - tx_start(); }

    explicit operator bool() const noexcept { return s_ != nullptr; }
    auto operator!() const noexcept -> bool { return s_ == nullptr; }

    auto get_cm_or_null() const noexcept -> std::shared_ptr<commit_manager>;

    private:
    type val_;
    std::shared_ptr<state_> s_;
  };

  private:
  ///\brief Shared state for a write operation.
  class monsoon_tx_export_ write_id_state_
  : public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::safe_link>>
  {
    friend commit_manager;

    public:
    write_id_state_() = default;
    explicit write_id_state_(commit_id seq, txfile::transaction&& tx) noexcept;

    ///\brief Allow commit_manager to take over ownership.
    static void on_pimpl_release(unique_alloc_ptr<write_id_state_, traits_type::rebind_alloc<write_id_state_>>&&) noexcept;
    ///\brief Retrieve the commit manager.
    auto get_cm_or_null() const noexcept -> std::shared_ptr<commit_manager>;
    ///\brief Apply changes.
    template<typename Validation, typename Phase2>
    auto apply(Validation&& validation, Phase2&& phase2) -> std::error_code;
    ///\brief Get the commit ID of the write operation.
    auto seq() const noexcept -> const commit_id& { return seq_; }
    ///\brief Add a write operation to this commit.
    auto write_at(txfile::transaction::offset_type offset, const void* buf, std::size_t nbytes) -> std::size_t;

    private:
    void wait_until_front_transaction_(const commit_manager& cm);

    commit_id seq_;
    txfile::transaction tx_;
    std::variant<std::monostate, std::condition_variable_any> wait_;
  };

  using state_list = boost::intrusive::list<
      state_,
      boost::intrusive::constant_time_size<false>>;

  using write_list = boost::intrusive::list<
      write_id_state_,
      boost::intrusive::constant_time_size<false>>;

  public:
  ///\brief Don't use the constructor, but use the allocate method instead.
  commit_manager(const txfile& f, monsoon::io::fd::offset_type off, allocator_type alloc);
  ~commit_manager() noexcept;

  static auto allocate(const txfile& f, monsoon::io::fd::offset_type off, allocator_type alloc = allocator_type()) -> std::shared_ptr<commit_manager>;
  static void init(txfile::transaction& tx, monsoon::io::fd::offset_type off);

  ///\brief Allocate a transaction ID.
  ///\details This is for transaction read operations only.
  auto get_tx_commit_id() const -> commit_id;
  ///\brief Allocate a transaction ID for writing.
  ///\details This transaction ID can be used to prepare transactions.
  ///\note Transactions are executed in order of commit ID.
  auto prepare_commit(txfile& f) -> write_id;

  private:
  ///\brief Accept the dropped write state.
  void null_commit_(unique_alloc_ptr<write_id_state_, traits_type::rebind_alloc<write_id_state_>> wis) noexcept;
  ///\brief See if we can start the front commit.
  void maybe_start_front_write_locked_(const std::unique_lock<std::shared_mutex>& lck) noexcept;

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
  ///\brief Commit ID for read transactions.
  ///\details Read transaction IDs are collapsed into a single ID.
  std::shared_ptr<state_> s_;
  ///\brief Most-recent handed-out ID for a commit.
  type last_write_commit_id_;
  ///\brief Number of last_write_commit_id_ we have pre-allocated.
  type last_write_commit_id_avail_ = 0;
  ///\brief Most-recent completed ID for a commit.
  type completed_commit_id_;
  ///\brief List of pending writes.
  ///\details The front of the list contains the first-to-be-written transaction.
  ///\note A write may not be ready to be written.
  write_list writes_;
  ///\brief Offset in txfile of this.
  monsoon::io::fd::offset_type off_;
  ///\brief Allocator.
  allocator_type alloc_;
};


class monsoon_tx_export_ commit_manager::write_id {
  friend commit_manager;

  public:
  constexpr write_id() noexcept = default;
  ~write_id() noexcept;

  private:
  explicit write_id(unique_alloc_ptr<write_id_state_, traits_type::rebind_alloc<write_id_state_>>&& impl) noexcept;

  public:
  ///\brief Apply changes.
  template<typename Validation, typename Phase2>
  auto apply(Validation&& validation, Phase2&& phase2) -> std::error_code;
  ///\brief Get the commit ID of the write operation.
  auto seq() const noexcept -> const commit_id& { return pimpl_->seq_; }
  ///\brief Add a write operation to this commit.
  auto write_at(txfile::transaction::offset_type offset, const void* buf, std::size_t nbytes) -> std::size_t;

  private:
  unique_alloc_ptr<write_id_state_, traits_type::rebind_alloc<write_id_state_>> pimpl_;
};


auto operator==(const commit_manager::commit_id& x, const commit_manager::commit_id& y) noexcept;
auto operator!=(const commit_manager::commit_id& x, const commit_manager::commit_id& y) noexcept;
auto operator< (const commit_manager::commit_id& x, const commit_manager::commit_id& y) noexcept;
auto operator> (const commit_manager::commit_id& x, const commit_manager::commit_id& y) noexcept;
auto operator<=(const commit_manager::commit_id& x, const commit_manager::commit_id& y) noexcept;
auto operator>=(const commit_manager::commit_id& x, const commit_manager::commit_id& y) noexcept;



} /* namespace monsoon::tx::detail */

#include "commit_id-inl.h"

#endif /* MONSOON_TX_DETAIL_COMMIT_ID_H */
