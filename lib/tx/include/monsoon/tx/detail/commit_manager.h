#ifndef MONSOON_TX_DETAIL_COMMIT_MANAGER_H
#define MONSOON_TX_DETAIL_COMMIT_MANAGER_H

#include <monsoon/tx/detail/export_.h>
#include <monsoon/tx/txfile.h>
#include <monsoon/unique_alloc_ptr.h>
#include <monsoon/shared_resource_allocator.h>
#include <monsoon/cheap_fn_ref.h>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <system_error>

namespace monsoon::tx::detail {


/**
 * \brief Interface for commit manager.
 */
class monsoon_tx_export_ commit_manager {
  public:
  static constexpr std::size_t SIZE = 16; // 16 byte on disk.

  ///\brief Allocator used by commit_manager.
  using allocator_type = shared_resource_allocator<std::byte>;
  ///\brief Allocator traits.
  using traits_type = std::allocator_traits<allocator_type>;

  ///\brief Raw commit ID type.
  using type = std::uint32_t;
  ///\brief Interface used during commits.
  class write_id;

  protected:
  class monsoon_tx_export_ state_ {
    protected:
    state_(type tx_start, type val) noexcept
    : tx_start(tx_start),
      val(val)
    {}

    ~state_() noexcept = default;

    public:
    ///\brief Retrieve the commit manager.
    virtual auto get_cm_or_null() const noexcept -> std::shared_ptr<commit_manager> = 0;

    const type tx_start, val;
  };

  public:
  class monsoon_tx_export_ commit_id {
    friend commit_manager;

    public:
    using type = commit_manager::type;

    commit_id() noexcept = default;

    private:
    commit_id(const std::shared_ptr<const state_>& s) noexcept;

    public:
    auto tx_start() const noexcept -> type { return s_->tx_start; }
    auto val() const noexcept -> type { return s_->val; }
    auto relative_val() const noexcept -> type { return val() - tx_start(); }

    explicit operator bool() const noexcept { return s_ != nullptr; }
    auto operator!() const noexcept -> bool { return s_ == nullptr; }

    auto get_cm_or_null() const noexcept -> std::shared_ptr<commit_manager>;

    private:
    std::shared_ptr<const state_> s_;
  };

  protected:
  ///\brief Shared state for a write operation.
  class monsoon_tx_export_ write_id_state_ {
    protected:
    write_id_state_(commit_id seq, txfile::transaction&& tx) noexcept;
    ~write_id_state_() noexcept = default;

    public:
    ///\brief Retrieve the commit manager.
    auto get_cm_or_null() const noexcept -> std::shared_ptr<commit_manager>;
    ///\brief Apply changes.
    template<typename Validation, typename Phase2>
    auto apply(Validation&& validation, Phase2&& phase2) -> std::error_code;
    ///\brief Get the commit ID of the write operation.
    auto seq() const noexcept -> const commit_id& { return seq_; }
    ///\brief Add a write operation to this commit.
    auto write_at(txfile::transaction::offset_type offset, const void* buf, std::size_t nbytes) -> std::size_t;
    ///\brief Add a write operation to this commit.
    void write_at_many(std::vector<txfile::transaction::offset_type> offsets, const void* buf, std::size_t nbytes);

    private:
    ///\brief Transaction apply implementation.
    virtual auto do_apply(cheap_fn_ref<std::error_code()> validation, cheap_fn_ref<> phase2) -> std::error_code = 0;

    protected:
    const commit_id seq_;
    txfile::transaction tx_;
  };

  protected:
  ///\brief Don't use the constructor, but use the allocate method instead.
  commit_manager(allocator_type alloc) : alloc_(std::move(alloc)) {}
  virtual ~commit_manager() noexcept = 0;

  public:
  ///\brief Load commit manager from \p f.
  ///\note Detects the commit manager type automatically, using its magic value.
  static auto allocate(const txfile& f, monsoon::io::fd::offset_type off, allocator_type alloc = allocator_type()) -> std::shared_ptr<commit_manager>;

  ///\brief Allocate a transaction ID.
  ///\details This is for transaction read operations only.
  auto get_tx_commit_id() const -> commit_id;
  ///\brief Allocate a transaction ID.
  ///\details This is for transaction read operations only.
  ///\param tx_alloc Allocator to use for transaction specific information.
  auto get_tx_commit_id(allocator_type tx_alloc) const -> commit_id;
  ///\brief Allocate a transaction ID for writing.
  ///\details This transaction ID can be used to prepare transactions.
  ///\note Transactions are executed in order of commit ID.
  ///\param f The file on which this commit_manager operates.
  auto prepare_commit(txfile& f) -> write_id;
  ///\brief Allocate a transaction ID for writing.
  ///\details This transaction ID can be used to prepare transactions.
  ///\note Transactions are executed in order of commit ID.
  ///\param f The file on which this commit_manager operates.
  ///\param tx_alloc Allocator to use for transaction specific information.
  auto prepare_commit(txfile& f, allocator_type tx_alloc) -> write_id;

  private:
  ///\brief Allocate a transaction ID.
  ///\details This is for transaction read operations only.
  ///\param tx_alloc Allocator to use for transaction specific information.
  virtual auto do_get_tx_commit_id_(allocator_type tx_alloc) const -> commit_id = 0;
  ///\brief Allocate a transaction ID for writing.
  ///\details This transaction ID can be used to prepare transactions.
  ///\note Transactions are executed in order of commit ID.
  ///\param f The file on which this commit_manager operates.
  ///\param tx_alloc Allocator to use for transaction specific information.
  virtual auto do_prepare_commit_(txfile& f, allocator_type tx_alloc) -> write_id = 0;

  protected:
  ///\brief Helper function to expose the commit_id constructor to derived types.
  static auto make_commit_id(const std::shared_ptr<const state_>& s) noexcept -> commit_id;
  ///\brief Helper function to expose the write_id constructor to derived types.
  static auto make_write_id(std::shared_ptr<write_id_state_>&& s) noexcept -> write_id;
  ///\brief Extract state from commit_id.
  static auto get_commit_id_state(const commit_id& cid) noexcept -> const std::shared_ptr<const state_>&;

  private:
  ///\brief Suggest a commit_id as the new tx_start_, for use by the vacuum algorithm.
  virtual auto suggest_vacuum_target_() const -> commit_id = 0;
  ///\brief Update after a vacuum operation completed.
  virtual void on_completed_vacuum_(txfile& f, commit_id vacuum_target) = 0;

  protected:
  ///\brief Allocator.
  allocator_type alloc_;
};


class monsoon_tx_export_ commit_manager::write_id {
  friend commit_manager;

  public:
  constexpr write_id() noexcept = default;
  write_id(const write_id&) = delete;
  write_id& operator=(const write_id&) = delete;
  write_id(write_id&&) = default;
  write_id& operator=(write_id&&) = default;
  ~write_id() noexcept = default;

  private:
  explicit write_id(std::shared_ptr<write_id_state_>&& impl) noexcept;

  public:
  ///\brief Apply changes.
  template<typename Validation, typename Phase2>
  auto apply(Validation&& validation, Phase2&& phase2) -> std::error_code;
  ///\brief Get the commit ID of the write operation.
  auto seq() const noexcept -> const commit_id& { return pimpl_->seq(); }
  ///\brief Add a write operation to this commit.
  auto write_at(txfile::transaction::offset_type offset, const void* buf, std::size_t nbytes) -> std::size_t;
  ///\brief Add a write operation to this commit.
  void write_at_many(std::vector<txfile::transaction::offset_type> offsets, const void* buf, std::size_t nbytes);

  private:
  std::shared_ptr<write_id_state_> pimpl_;
};


auto operator==(const commit_manager::commit_id& x, const commit_manager::commit_id& y) noexcept;
auto operator!=(const commit_manager::commit_id& x, const commit_manager::commit_id& y) noexcept;
auto operator< (const commit_manager::commit_id& x, const commit_manager::commit_id& y) noexcept;
auto operator> (const commit_manager::commit_id& x, const commit_manager::commit_id& y) noexcept;
auto operator<=(const commit_manager::commit_id& x, const commit_manager::commit_id& y) noexcept;
auto operator>=(const commit_manager::commit_id& x, const commit_manager::commit_id& y) noexcept;

template<typename CharT, typename Traits>
auto operator<<(std::basic_ostream<CharT, Traits>& out, const commit_manager::commit_id& ci) -> std::basic_ostream<CharT, Traits>&;


} /* namespace monsoon::tx::detail */

#include "commit_manager-inl.h"

#endif /* MONSOON_TX_DETAIL_COMMIT_MANAGER_H */
