#ifndef MONSOON_TX_DETAIL_COMMIT_ID_INL_H
#define MONSOON_TX_DETAIL_COMMIT_ID_INL_H

#include <monsoon/tx/db_errc.h>
#include <utility>

namespace monsoon::tx::detail {


inline auto commit_manager::make_commit_id(type val, const std::shared_ptr<state_>& s) noexcept -> commit_id {
  return commit_id(val, s);
}

inline auto commit_manager::make_write_id(std::shared_ptr<write_id_state_>&& s) noexcept -> write_id {
  return write_id(std::move(s));
}


inline commit_manager::commit_id::commit_id(type val, const std::shared_ptr<state_>& s) noexcept
: val_(val),
  s_(s)
{}

inline auto commit_manager::commit_id::get_cm_or_null() const noexcept -> std::shared_ptr<commit_manager> {
  return (s_ == nullptr ? nullptr : s_->get_cm_or_null());
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

  return do_apply(std::forward<Validation>(validation), std::forward<Phase2>(phase2));
}

inline auto commit_manager::write_id_state_::write_at(txfile::transaction::offset_type offset, const void* buf, std::size_t nbytes) -> std::size_t {
  return tx_.write_at(offset, buf, nbytes);
}

inline void commit_manager::write_id_state_::write_at_many(std::vector<txfile::transaction::offset_type> offsets, const void* buf, std::size_t nbytes) {
  tx_.write_at_many(std::move(offsets), buf, nbytes);
}


inline commit_manager::write_id::write_id(std::shared_ptr<write_id_state_>&& impl) noexcept
: pimpl_(std::move(impl))
{}

template<typename Validation, typename Phase2>
inline auto commit_manager::write_id::apply(Validation&& validation, Phase2&& phase2) -> std::error_code {
  assert(pimpl_ != nullptr);
  return pimpl_->apply(std::forward<Validation>(validation), std::forward<Phase2>(phase2));
}

inline auto commit_manager::write_id::write_at(txfile::transaction::offset_type offset, const void* buf, std::size_t nbytes) -> std::size_t {
  assert(pimpl_ != nullptr);
  return pimpl_->write_at(offset, buf, nbytes);
}

inline void commit_manager::write_id::write_at_many(std::vector<txfile::transaction::offset_type> offsets, const void* buf, std::size_t nbytes) {
  assert(pimpl_ != nullptr);
  return pimpl_->write_at_many(std::move(offsets), buf, nbytes);
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
