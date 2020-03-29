#include <monsoon/tx/detail/tx_op.h>
#include <algorithm>
#include <cassert>
#include <iterator>

namespace monsoon::tx::detail {


tx_op::~tx_op() noexcept {
  assert(completed_);
}

void tx_op::commit() noexcept {
  assert(!completed_);
  if (!std::exchange(completed_, true)) commit_();
}

void tx_op::rollback() noexcept {
  assert(!completed_);
  if (!std::exchange(completed_, true)) rollback_();
}

void tx_op::before_destroy_() noexcept {
  if (!std::exchange(completed_, true)) rollback_();
}


void tx_op_collection::reserve(size_type new_cap) {
  ops_.reserve(new_cap);
}

void tx_op_collection::push(std::shared_ptr<tx_op> op) {
  push_back(std::move(op));
}

void tx_op_collection::push_back(std::shared_ptr<tx_op> op) {
  ops_.push_back(std::move(op));
}

auto tx_op_collection::operator+=(tx_op_collection&& y) -> tx_op_collection& {
  const auto oldsize = ops_.size();
  try {
    ops_.insert(ops_.end(), y.ops_.begin(), y.ops_.end());
  } catch (...) {
    while (ops_.size() > oldsize) ops_.pop_back(); // Never throws.
    throw;
  }
  y.ops_.clear();
  return *this;
}

void tx_op_collection::commit() noexcept {
  std::for_each(ops_.cbegin(), ops_.cend(), [](const auto& op) { op->commit(); });
  ops_.clear();
}

void tx_op_collection::rollback() noexcept {
  std::for_each(ops_.crbegin(), ops_.crend(), [](const auto& op) { op->rollback(); });
  ops_.clear();
}


tx_op::impl_<std::nullptr_t, std::nullptr_t>::~impl_() noexcept = default;

void tx_op::impl_<std::nullptr_t, std::nullptr_t>::commit_() noexcept {}

void tx_op::impl_<std::nullptr_t, std::nullptr_t>::rollback_() noexcept {}


} /* namespace monsoon::tx::detail */
