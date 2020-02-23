#ifndef MONSOON_TX_DB_INL_H
#define MONSOON_TX_DB_INL_H

#include <utility>

namespace monsoon::tx {


inline db::transaction::transaction(transaction&& other) noexcept
: seq_(std::move(other.seq_)),
  read_only_(std::move(other.read_only_)),
  active_(std::exchange(other.active_, false)),
  callbacks_(std::move(other.callbacks_)),
  f_(std::exchange(other.f_, nullptr))
{}

inline auto db::transaction::operator=(transaction&& other) noexcept -> transaction& {
  if (active_) rollback();

  seq_ = std::move(other.seq_);
  read_only_ = std::move(other.read_only_);
  active_ = std::exchange(other.active_, false);
  callbacks_ = std::move(other.callbacks_);
  f_ = std::exchange(other.f_, nullptr);
  return *this;
}

inline db::transaction::~transaction() noexcept {
  if (active_) rollback();
}

template<typename T>
inline auto db::transaction::on(cycle_ptr::cycle_gptr<T> v) -> cycle_ptr::cycle_gptr<typename T::tx_object> {
  auto& txo = callbacks_[v];
  if (txo == nullptr) txo = cycle_ptr::make_cycle<typename T::tx_object>(*this, std::move(v));

#ifdef NDEBUG
  return std::static_pointer_cast<typename T::tx_object>(txo)
#else
  auto result_ptr = std::dynamic_pointer_cast<typename T::tx_object>(txo);
  assert(result_ptr != nullptr); // Means the types are incorrect.
  return result_ptr;
#endif
}

inline db::transaction::transaction(seq_type seq, bool read_only, txfile& f) noexcept
: seq_(seq),
  read_only_(read_only),
  active_(true),
  f_(&f)
{}

inline auto db::transaction::before(const transaction& other) const noexcept -> bool {
  return before(seq_, other.seq_);
}

inline auto db::transaction::after(const transaction& other) const noexcept -> bool {
  return other.before(*this);
}


} /* namespace monsoon::tx */

#endif /* MONSOON_TX_DB_INL_H */
