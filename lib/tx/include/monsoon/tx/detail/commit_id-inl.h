#ifndef MONSOON_TX_DETAIL_COMMIT_ID_INL_H
#define MONSOON_TX_DETAIL_COMMIT_ID_INL_H

#include <monsoon/tx/db_errc.h>
#include <utility>

namespace monsoon::tx::detail {


inline commit_manager::state_::~state_() noexcept {
  const auto cm_ptr = cm.lock();
  if (cm_ptr != nullptr) {
    std::lock_guard<std::shared_mutex> lck{ cm_ptr->mtx_ };
    cm_ptr->states_.erase(cm_ptr->states_.iterator_to(*this));
  }
}


inline commit_manager::commit_id::commit_id(type val, const std::shared_ptr<state_>& s) noexcept
: val_(val),
  s_(s)
{}

inline auto commit_manager::commit_id::get_cm_or_null() const noexcept -> std::shared_ptr<commit_manager> {
  return (s_ == nullptr ? nullptr : s_->cm.lock());
}


inline commit_manager::write_id_state_::write_id_state_(commit_id seq, txfile::transaction&& tx) noexcept
: seq_(std::move(seq)),
  tx_(std::move(tx))
{}

inline auto commit_manager::write_id_state_::get_cm_or_null() const noexcept -> std::shared_ptr<commit_manager> {
  return seq_.get_cm_or_null();
}

template<typename Validation, typename Phase2>
inline auto commit_manager::write_id_state_::apply(Validation&& validation, Phase2&& phase2) -> std::error_code {
  static_assert(std::is_invocable_r_v<std::error_code, Validation>,
      "validation must be a no-argument function returning an error code");
  static_assert(std::is_nothrow_invocable_v<Phase2>,
      "phase2 must be a no-argument function that never throws");

  const auto cm = get_cm_or_null();
  if (cm == nullptr) return db_errc::gone_away;
  wait_until_front_transaction_(*cm);

  std::unique_lock<std::shared_mutex> commit_lck{ cm->commit_mtx_ };

  std::error_code ec = std::invoke(std::forward<Validation>(validation));
  if (ec) return ec; // Validation failure.

  std::unique_lock<std::shared_mutex> wlck{ cm->mtx_ };
  tx_.commit(); // May throw.

  // Remaining code runs under no-except clause to maintain invariants.
  auto nothrow_stage = [&]() noexcept -> std::error_code {
    // Update all in-memory data structures.
    phase2();

    // Now that all in-memory and all on-disk data structures have the commit,
    // we can update the in-memory completed-commit-ID.
    cm->completed_commit_id_ = seq_.val();
    cm->writes_.erase(cm->writes_.iterator_to(*this));

    // We no longer need to prevent other transactions from running.
    commit_lck.unlock();

    // Maybe pick up a new transaction.
    cm->maybe_start_front_write_locked_(wlck);
    return {};
  };
  return nothrow_stage();
}

inline auto commit_manager::write_id_state_::write_at(txfile::transaction::offset_type offset, const void* buf, std::size_t nbytes) -> std::size_t {
  return tx_.write_at(offset, buf, nbytes);
}


inline commit_manager::write_id::write_id(unique_alloc_ptr<write_id_state_, traits_type::rebind_alloc<write_id_state_>>&& impl) noexcept
: pimpl_(std::move(impl))
{}

inline commit_manager::write_id::~write_id() noexcept {
  // Give transaction manager a chance to prevent destruction.
  if (pimpl_ != nullptr)
    pimpl_->on_pimpl_release(std::move(pimpl_));
}

template<typename Validation, typename Phase2>
inline auto commit_manager::write_id::apply(Validation&& validation, Phase2&& phase2) -> std::error_code {
  assert(pimpl_ != nullptr);
  return pimpl_->apply(std::forward<Validation>(validation), std::forward<Phase2>(phase2));
}

inline auto commit_manager::write_id::write_at(txfile::transaction::offset_type offset, const void* buf, std::size_t nbytes) -> std::size_t {
  assert(pimpl_ != nullptr);
  return pimpl_->write_at(offset, buf, nbytes);
}


inline auto operator==(const commit_manager::commit_id& x, const commit_manager::commit_id& y) noexcept {
  if (x && y) {
    return x.relative_val() == y.val() - x.tx_start();
  } else {
    return !x && !y;
  }
}

inline auto operator!=(const commit_manager::commit_id& x, const commit_manager::commit_id& y) noexcept {
  return !(x == y);
}

inline auto operator<(const commit_manager::commit_id& x, const commit_manager::commit_id& y) noexcept {
  if (x && y) {
    return x.relative_val() < y.val() - x.tx_start();
  } else {
    return !x && y;
  }
}

inline auto operator>(const commit_manager::commit_id& x, const commit_manager::commit_id& y) noexcept {
  return y < x;
}

inline auto operator<=(const commit_manager::commit_id& x, const commit_manager::commit_id& y) noexcept {
  return !(y < x);
}

inline auto operator>=(const commit_manager::commit_id& x, const commit_manager::commit_id& y) noexcept {
  return !(x < y);
}


} /* namespace monsoon::tx::detail */

#endif /* MONSOON_TX_DETAIL_COMMIT_ID_INL_H */
