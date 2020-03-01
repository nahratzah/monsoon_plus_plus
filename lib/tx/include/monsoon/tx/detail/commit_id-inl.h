#ifndef MONSOON_TX_DETAIL_COMMIT_ID_INL_H
#define MONSOON_TX_DETAIL_COMMIT_ID_INL_H

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

inline auto commit_manager::write_id::apply(std::function<std::error_code()> validation, std::function<void()> phase2) -> std::error_code {
  assert(pimpl_ != nullptr);
  return pimpl_->apply(std::move(validation), std::move(phase2));
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
