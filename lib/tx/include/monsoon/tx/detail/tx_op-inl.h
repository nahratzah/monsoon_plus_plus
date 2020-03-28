#ifndef MONSOON_TX_DETAIL_TX_OP_INL_H
#define MONSOON_TX_DETAIL_TX_OP_INL_H

#include <functional>

namespace monsoon::tx::detail {


template<typename Alloc, typename CommitFn, typename RollbackFn>
auto allocate_tx_op(Alloc&& alloc, CommitFn&& commit_fn, RollbackFn&& rollback_fn) -> std::shared_ptr<tx_op> {
  using tx_op_impl = tx_op::impl_<std::decay_t<CommitFn>, std::decay_t<RollbackFn>>;

  try {
    return std::allocate_shared<tx_op_impl>(std::forward<Alloc>(alloc), std::forward<CommitFn>(commit_fn), rollback_fn);
  } catch (...) {
    // When we fail, ensure we still invoke the rollback function.
    if constexpr(!std::is_same_v<std::nullptr_t, std::decay_t<RollbackFn>>)
      std::invoke(rollback_fn);
    throw;
  }
}

template<typename CommitFn, typename RollbackFn>
auto make_tx_op(CommitFn&& commit_fn, RollbackFn&& rollback_fn) -> std::shared_ptr<tx_op> {
  using tx_op_impl = tx_op::impl_<std::decay_t<CommitFn>, std::decay_t<RollbackFn>>;

  try {
    return std::make_shared<tx_op_impl>(std::forward<CommitFn>(commit_fn), rollback_fn);
  } catch (...) {
    // When we fail, ensure we still invoke the rollback function.
    if constexpr(!std::is_same_v<std::nullptr_t, std::decay_t<RollbackFn>>)
      std::invoke(rollback_fn);
    throw;
  }
}


template<typename CommitFn, typename RollbackFn>
inline tx_op::impl_<CommitFn, RollbackFn>::impl_(CommitFn commit_fn, RollbackFn rollback_fn)
: commit_fn_(std::move(commit_fn)),
  rollback_fn_(std::move(rollback_fn))
{}

template<typename CommitFn, typename RollbackFn>
inline tx_op::impl_<CommitFn, RollbackFn>::~impl_() noexcept {
  before_destroy_();
}

template<typename CommitFn, typename RollbackFn>
inline void tx_op::impl_<CommitFn, RollbackFn>::commit_() noexcept {
  std::invoke(std::move(commit_fn_));
}

template<typename CommitFn, typename RollbackFn>
inline void tx_op::impl_<CommitFn, RollbackFn>::rollback_() noexcept {
  std::invoke(std::move(rollback_fn_));
}


template<typename CommitFn>
inline tx_op::impl_<CommitFn, std::nullptr_t>::impl_(CommitFn commit_fn, [[maybe_unused]] std::nullptr_t rollback_fn)
: commit_fn_(std::move(commit_fn))
{}

template<typename CommitFn>
inline tx_op::impl_<CommitFn, std::nullptr_t>::~impl_() noexcept {
  before_destroy_();
}

template<typename CommitFn>
inline void tx_op::impl_<CommitFn, std::nullptr_t>::commit_() noexcept {
  std::invoke(std::move(commit_fn_));
}

template<typename CommitFn>
inline void tx_op::impl_<CommitFn, std::nullptr_t>::rollback_() noexcept {}


template<typename RollbackFn>
inline tx_op::impl_<std::nullptr_t, RollbackFn>::impl_([[maybe_unused]] std::nullptr_t commit_fn, RollbackFn rollback_fn)
: rollback_fn_(std::move(rollback_fn))
{}

template<typename RollbackFn>
inline tx_op::impl_<std::nullptr_t, RollbackFn>::~impl_() noexcept {
  before_destroy_();
}

template<typename RollbackFn>
inline void tx_op::impl_<std::nullptr_t, RollbackFn>::commit_() noexcept {}

template<typename RollbackFn>
inline void tx_op::impl_<std::nullptr_t, RollbackFn>::rollback_() noexcept {
  std::invoke(std::move(rollback_fn_));
}


inline tx_op_collection::tx_op_collection(allocator_type alloc)
: ops_(std::move(alloc))
{}

inline auto tx_op_collection::empty() const noexcept -> bool {
  return ops_.empty();
}

inline auto tx_op_collection::size() const noexcept -> size_type {
  return ops_.size();
}

inline auto tx_op_collection::capacity() const noexcept -> size_type {
  return ops_.capacity();
}


} /* namespace monsoon::tx::detail */

#endif /* MONSOON_TX_DETAIL_TX_OP_INL_H */
