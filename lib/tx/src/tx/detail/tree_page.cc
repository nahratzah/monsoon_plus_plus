#include <monsoon/tx/detail/tree_page.h>

namespace monsoon::tx::detail {


abstract_tree_elem::~abstract_tree_elem() noexcept = default;

auto abstract_tree_elem::lock_parent_for_read() const
-> std::tuple<cycle_ptr::cycle_gptr<abstract_leaf_page>, std::shared_lock<std::shared_mutex>> {
  for (;;) {
    std::shared_lock<std::shared_mutex> self_lck{ mtx_ };
    cycle_ptr::cycle_gptr<abstract_leaf_page> p = parent_;
    std::shared_lock<std::shared_mutex> p_lck(p->mtx_, std::try_to_lock);
    if (p_lck.owns_lock()) return std::make_tuple(std::move(p), std::move(p_lck));

    self_lck.unlock();
    p_lck.lock();
    self_lck.lock();
    if (p == parent_) return std::make_tuple(std::move(p), std::move(p_lck));
  }
}

auto abstract_tree_elem::lock_parent_for_write() const
-> std::tuple<cycle_ptr::cycle_gptr<abstract_leaf_page>, std::unique_lock<std::shared_mutex>> {
  for (;;) {
    std::shared_lock<std::shared_mutex> self_lck{ mtx_ };
    cycle_ptr::cycle_gptr<abstract_leaf_page> p = parent_;
    std::unique_lock<std::shared_mutex> p_lck(p->mtx_, std::try_to_lock);
    if (p_lck.owns_lock()) return std::make_tuple(std::move(p), std::move(p_lck));

    self_lck.unlock();
    p_lck.lock();
    self_lck.lock();
    if (p == parent_) return std::make_tuple(std::move(p), std::move(p_lck));
  }
}


abstract_leaf_page::~abstract_leaf_page() noexcept = default;


abstract_node_page::~abstract_node_page() noexcept = default;


} /* namespace monsoon::tx::detail */
