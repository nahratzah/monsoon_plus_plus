#ifndef MONSOON_TX_DETAIL_TX_OP_H
#define MONSOON_TX_DETAIL_TX_OP_H

#include <monsoon/tx/detail/export_.h>
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
  ///\param rollback_fn Functor object invoked during rollback.
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


} /* namespace monsoon::tx::detail */

#include "tx_op-inl.h"

#endif /* MONSOON_TX_DETAIL_TX_OP_H */
