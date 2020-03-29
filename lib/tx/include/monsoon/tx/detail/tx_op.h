#ifndef MONSOON_TX_DETAIL_TX_OP_H
#define MONSOON_TX_DETAIL_TX_OP_H

#include <monsoon/tx/detail/export_.h>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <vector>
#include <monsoon/shared_resource_allocator.h>

namespace monsoon::tx::detail {


/**
 * \brief A tiny transactional modification to a memory object.
 * \details
 * For an operation, maintains the commit and rollback side of the operation,
 * so that the operation can complete properly.
 *
 * The class guarantees that exactly one of the commit or rollback implementations
 * will be invoked exactly once.
 * (And the other exactly never.)
 */
class monsoon_tx_export_ tx_op {
  ///\brief Create a new tx_op using the given allocator.
  ///\param alloc Allocator used to allocate memory.
  ///\param commit_fn Functor object invoked during commit.
  ///If nullptr, then the tx_op shall have a noop commit function.
  ///\param rollback_fn Functor object invoked during rollback.
  ///If nullptr, then the tx_op shall have a noop rollback function.
  ///\return A tx_op that will invoke the commit or rollback function appropriately.
  template<typename Alloc, typename CommitFn, typename RollbackFn>
  friend auto allocate_tx_op(Alloc&& alloc, CommitFn&& commit_fn, RollbackFn&& rollback_fn) -> std::shared_ptr<tx_op>;

  ///\brief Create a new tx_op.
  ///\param commit_fn Functor object invoked during commit.
  ///\param rollback_fn Functor object invoked during rollback.
  ///\return A tx_op that will invoke the commit or rollback function appropriately.
  template<typename CommitFn, typename RollbackFn>
  friend auto make_tx_op(CommitFn&& commit_fn, RollbackFn&& rollback_fn) -> std::shared_ptr<tx_op>;

  private:
  template<typename CommitFn, typename RollbackFn> class impl_;

  public:
  tx_op() noexcept = default;
  tx_op(const tx_op&) = delete;
  tx_op(tx_op&&) = delete;
  tx_op& operator=(const tx_op&) = delete;
  tx_op& operator=(tx_op&&) = delete;
  virtual ~tx_op() noexcept = 0;

  void commit() noexcept;
  void rollback() noexcept;

  protected:
  void before_destroy_() noexcept;

  private:
  virtual void commit_() noexcept = 0;
  virtual void rollback_() noexcept = 0;

  bool completed_ = false;
};


/**
 * \brief A collection of tx_op objects.
 * \details
 * Holds zero or more tx_op objects.
 */
class monsoon_tx_export_ tx_op_collection {
  private:
  using collection_type = std::vector<std::shared_ptr<tx_op>, shared_resource_allocator<std::shared_ptr<tx_op>>>;

  public:
  using allocator_type = shared_resource_allocator<std::byte>;
  using size_type = collection_type::size_type;

  tx_op_collection() = default;
  explicit tx_op_collection(allocator_type alloc);
  tx_op_collection(const tx_op_collection&) = delete;
  tx_op_collection(tx_op_collection&&) = default;
  tx_op_collection& operator=(const tx_op_collection&) = delete;
  tx_op_collection& operator=(tx_op_collection&&) = default;

  auto empty() const noexcept -> bool;
  auto size() const noexcept -> size_type;
  auto capacity() const noexcept -> size_type;
  void reserve(size_type new_cap);

  void push(std::shared_ptr<tx_op> op);
  void push_back(std::shared_ptr<tx_op> op);
  auto operator+=(tx_op_collection&& y) -> tx_op_collection&;

  void commit() noexcept;
  void rollback() noexcept;

  auto get_allocator() const -> allocator_type;

  ///\brief Add a commit hook.
  template<typename CommitFn>
  auto on_commit(CommitFn&& commit_fn) -> tx_op_collection&;
  ///\brief Add a rollback hook.
  template<typename RollbackFn>
  auto on_rollback(RollbackFn&& rollback_fn) -> tx_op_collection&;
  ///\brief Add a commit and rollback hook.
  template<typename CommitFn, typename RollbackFn>
  auto on_complete(CommitFn&& commit_fn, RollbackFn&& rollback_fn) -> tx_op_collection&;

  private:
  collection_type ops_;
};


template<typename CommitFn, typename RollbackFn>
class tx_op::impl_
: public tx_op
{
  public:
  impl_(CommitFn commit_fn, RollbackFn rollback_fn);
  ~impl_() noexcept override;

  private:
  void commit_() noexcept override;
  void rollback_() noexcept override;

  CommitFn commit_fn_;
  RollbackFn rollback_fn_;
};


template<typename CommitFn>
class tx_op::impl_<CommitFn, std::nullptr_t>
: public tx_op
{
  public:
  impl_(CommitFn commit_fn, [[maybe_unused]] std::nullptr_t rollback_fn);
  ~impl_() noexcept override;

  private:
  void commit_() noexcept override;
  void rollback_() noexcept override;

  CommitFn commit_fn_;
};


template<typename RollbackFn>
class tx_op::impl_<std::nullptr_t, RollbackFn>
: public tx_op
{
  public:
  impl_([[maybe_unused]] std::nullptr_t commit_fn, RollbackFn rollback_fn);
  ~impl_() noexcept override;

  private:
  void commit_() noexcept override;
  void rollback_() noexcept override;

  RollbackFn rollback_fn_;
};


template<>
class tx_op::impl_<std::nullptr_t, std::nullptr_t>
: public tx_op
{
  public:
  impl_([[maybe_unused]] std::nullptr_t commit_fn, [[maybe_unused]] std::nullptr_t rollback_fn) noexcept {}
  ~impl_() noexcept override;

  private:
  void commit_() noexcept override;
  void rollback_() noexcept override;
};


} /* namespace monsoon::tx::detail */

#include "tx_op-inl.h"

#endif /* MONSOON_TX_DETAIL_TX_OP_H */
