#ifndef MONSOON_TX_DB_INL_H
#define MONSOON_TX_DB_INL_H

#include <utility>
#include <boost/polymorphic_pointer_cast.hpp>

namespace monsoon::tx {


inline db::transaction::transaction(transaction&& other) noexcept
: seq_(std::move(other.seq_)),
  read_only_(std::move(other.read_only_)),
  active_(std::exchange(other.active_, false)),
  callbacks_(std::move(other.callbacks_)),
  self_(std::move(other.self_)),
  deleted_set_(std::move(other.deleted_set_)),
  created_set_(std::move(other.created_set_)),
  require_set_(std::move(other.require_set_))
{}

inline auto db::transaction::operator=(transaction&& other) noexcept -> transaction& {
  if (active_) rollback();

  seq_ = std::move(other.seq_);
  read_only_ = std::move(other.read_only_);
  active_ = std::exchange(other.active_, false);
  callbacks_ = std::move(other.callbacks_);
  self_ = std::move(other.self_);
  deleted_set_ = std::move(other.deleted_set_);
  created_set_ = std::move(other.created_set_);
  require_set_ = std::move(other.require_set_);
  return *this;
}

inline db::transaction::~transaction() noexcept {
  if (active_) rollback();
}

template<typename T>
inline auto db::transaction::on(cycle_ptr::cycle_gptr<T> v) -> cycle_ptr::cycle_gptr<typename T::tx_object> {
  auto& txo = callbacks_[v];
  if (txo == nullptr) txo = cycle_ptr::make_cycle<typename T::tx_object>(*this, std::move(v));
  return boost::polymorphic_pointer_downcast<typename T::tx_object>(txo);
}

inline db::transaction::transaction(detail::commit_manager::commit_id seq, bool read_only, db& self) noexcept
: seq_(seq),
  read_only_(read_only),
  active_(true),
  self_(self.shared_from_this())
{}

inline auto db::transaction::before(const transaction& other) const noexcept -> bool {
  return seq_ < other.seq_;
}

inline auto db::transaction::after(const transaction& other) const noexcept -> bool {
  return seq_ > other.seq_;
}


inline db::db_obj::db_obj(std::shared_ptr<class db> db)
: obj_cache(db->obj_cache_),
  db_(db)
{}


} /* namespace monsoon::tx */

#endif /* MONSOON_TX_DB_INL_H */
