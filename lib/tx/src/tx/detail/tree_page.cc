#include <monsoon/tx/detail/tree_page.h>
#include <monsoon/tx/tree_error.h>
#include <monsoon/io/rw.h>
#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <boost/endian/conversion.hpp>
#include <boost/polymorphic_cast.hpp>

namespace monsoon::tx::detail {


void tree_page_leaf::header::native_to_big_endian() noexcept {
  static_assert(sizeof(header) == header::SIZE);

  boost::endian::native_to_big_inplace(magic);
  boost::endian::native_to_big_inplace(flags);
  boost::endian::native_to_big_inplace(parent_off);
  boost::endian::native_to_big_inplace(next_sibling_off);
  boost::endian::native_to_big_inplace(prev_sibling_off);
}

void tree_page_leaf::header::big_to_native_endian() noexcept {
  static_assert(sizeof(header) == header::SIZE);

  boost::endian::big_to_native_inplace(magic);
  boost::endian::big_to_native_inplace(flags);
  boost::endian::big_to_native_inplace(parent_off);
  boost::endian::big_to_native_inplace(next_sibling_off);
  boost::endian::big_to_native_inplace(prev_sibling_off);
}

void tree_page_leaf::header::encode(boost::asio::mutable_buffer buf) const {
  assert(buf.size() >= SIZE);
  assert(magic == tree_page_leaf::magic);

  header tmp = *this;
  tmp.native_to_big_endian();
  boost::asio::buffer_copy(buf, boost::asio::const_buffer(&tmp, sizeof(tmp)));
}

void tree_page_leaf::header::decode(boost::asio::const_buffer buf) {
  assert(buf.size() >= SIZE);

  boost::asio::buffer_copy(boost::asio::buffer(this, sizeof(*this)), buf);
  big_to_native_endian();
  if (magic != tree_page_leaf::magic) throw tree_error("bad tree page magic");
}


void tree_page_branch::header::native_to_big_endian() noexcept {
  static_assert(sizeof(header) == header::SIZE);

  boost::endian::native_to_big_inplace(magic);
  boost::endian::native_to_big_inplace(size);
  boost::endian::native_to_big_inplace(parent_off);
}

void tree_page_branch::header::big_to_native_endian() noexcept {
  static_assert(sizeof(header) == header::SIZE);

  boost::endian::big_to_native_inplace(magic);
  boost::endian::big_to_native_inplace(size);
  boost::endian::big_to_native_inplace(parent_off);
}

void tree_page_branch::header::encode(boost::asio::mutable_buffer buf) const {
  assert(buf.size() >= SIZE);
  assert(magic == tree_page_branch::magic);

  header tmp = *this;
  tmp.native_to_big_endian();
  boost::asio::buffer_copy(buf, boost::asio::const_buffer(&tmp, sizeof(tmp)));
}

void tree_page_branch::header::decode(boost::asio::const_buffer buf) {
  assert(buf.size() >= SIZE);

  boost::asio::buffer_copy(boost::asio::buffer(this, sizeof(*this)), buf);
  big_to_native_endian();
  if (magic != tree_page_branch::magic) throw tree_error("bad tree page magic");
}


abstract_tree::~abstract_tree() noexcept = default;

auto abstract_tree::get_if_present(std::uint64_t off) const noexcept -> cycle_ptr::cycle_gptr<abstract_tree_page> {
  return boost::polymorphic_pointer_downcast<abstract_tree_page>(
      db_cache_->get_if_present(off, this->shared_from_this(this)));
}

auto abstract_tree::get(std::uint64_t off) const -> cycle_ptr::cycle_gptr<abstract_tree_page> {
  return boost::polymorphic_pointer_downcast<abstract_tree_page>(
      db_cache_->get(
          off, this->shared_from_this(this),
          [this](auto&& alloc, auto&& off) {
            return abstract_tree_page::decode(
                const_cast<abstract_tree&>(*this),
                f_->begin(),
                std::forward<decltype(off)>(off),
                std::forward<decltype(alloc)>(alloc));
          }));
}

void abstract_tree::invalidate(std::uint64_t off) const noexcept {
  db_cache_->invalidate(off, this->shared_from_this(this));
}

auto abstract_tree::allocate_leaf_(allocator_type allocator) -> cycle_ptr::cycle_gptr<tree_page_leaf> {
  return cycle_ptr::allocate_cycle<tree_page_leaf>(allocator, this->shared_from_this(this), allocator);
}

auto abstract_tree::allocate_branch_(allocator_type allocator) -> cycle_ptr::cycle_gptr<tree_page_branch> {
  return cycle_ptr::allocate_cycle<tree_page_branch>(allocator, this->shared_from_this(this), allocator);
}

struct monsoon_tx_local_ abstract_tree::page_with_lock {
  page_with_lock() noexcept = default;
  page_with_lock(page_with_lock&&) noexcept = default;
  page_with_lock& operator=(page_with_lock&&) noexcept = default;
  page_with_lock([[maybe_unused]] std::nullptr_t np) noexcept {}

  page_with_lock(cycle_ptr::cycle_gptr<abstract_tree_page> page)
  : page(std::move(page))
  {
    if (this->page != nullptr)
      lck = std::shared_lock<std::shared_mutex>(this->page->mtx_());
  }

  page_with_lock(cycle_ptr::cycle_gptr<abstract_tree_page> page, std::try_to_lock_t t)
  : page(std::move(page))
  {
    if (this->page != nullptr)
      lck = std::shared_lock<std::shared_mutex>(this->page->mtx_(), t);
  }

  auto operator=(cycle_ptr::cycle_gptr<abstract_tree_page> page) -> page_with_lock& {
    return *this = page_with_lock(std::move(page));
  }

  auto reset() {
    if (lck.owns_lock()) lck.unlock();
    page.reset();
  }

  cycle_ptr::cycle_gptr<abstract_tree_page> page;
  std::shared_lock<std::shared_mutex> lck;
};

auto abstract_tree::first_element_() -> cycle_ptr::cycle_gptr<abstract_tree_elem> {
  std::shared_lock<std::shared_mutex> lck{ mtx_ };
  if (root_off_ == 0) return nullptr;
  page_with_lock pwl = get(root_off_);
  lck.unlock();

  // Descend down the tree of pages.
  while (const auto branch_page = std::dynamic_pointer_cast<tree_page_branch>(pwl.page))
    pwl = get(branch_page->elems_.front()->off);

  // If it isn't a branch, then it is a leaf.
  for (auto leaf_page = boost::polymorphic_pointer_downcast<tree_page_leaf>(pwl.page);
       leaf_page != nullptr;
       pwl = leaf_page = leaf_page->next(pwl.lck)) {
    // Find the first non-nul element in the leaf.
    auto elem_iter = std::find_if(
        leaf_page->elems_.begin(), leaf_page->elems_.end(),
        [](const auto& ptr) { return ptr != nullptr; });
    if (elem_iter != leaf_page->elems_.end()) return *elem_iter;
  }
  // None of the leaves had any non-null elements.
  return nullptr;
}

auto abstract_tree::last_element_() -> cycle_ptr::cycle_gptr<abstract_tree_elem> {
  for (;;) {
    std::shared_lock<std::shared_mutex> lck{ mtx_ };
    if (root_off_ == 0) return nullptr;
    page_with_lock pwl = get(root_off_);
    lck.unlock();

    // Descend down the tree of pages.
    while (const auto branch_page = std::dynamic_pointer_cast<tree_page_branch>(pwl.page))
      pwl = get(branch_page->elems_.back()->off);

    // If it isn't a branch, then it is a leaf.
    bool validation_needed = false;
    cycle_ptr::cycle_gptr<abstract_tree_elem> result;
    while (auto leaf_page = boost::polymorphic_pointer_downcast<tree_page_leaf>(pwl.page)) {
      // Find the last non-null element in the leaf.
      auto elem_iter = std::find_if(
          leaf_page->elems_.rbegin(), leaf_page->elems_.rend(),
          [](const auto& ptr) { return ptr != nullptr; });
      if (elem_iter != leaf_page->elems_.rend()) {
        result = *elem_iter;
        break;
      }

      // If we fail to find any elements in this leaf, switch to the predecessor leaf.
      page_with_lock pwl_prev(leaf_page->prev(pwl.lck), std::try_to_lock);
      if (pwl_prev.page != nullptr && !pwl_prev.lck.owns_lock()) {
        pwl.lck.unlock();
        pwl_prev.lck.lock();
        if (!validation_needed) { // Maybe this caused invalidation.
          pwl.lck.lock();
          if (leaf_page->prev(pwl.lck) != pwl_prev.page) validation_needed = true;
        }
      }
      pwl = std::move(pwl_prev);
    }

    if (!validation_needed) return result;
    pwl.reset(); // Release locks so that calls won't harm us.
    if (result == nullptr) {
      if (first_element_() == nullptr) return result;
    } else {
      // We can fix this up by searching forward.
      for (auto result_succ = result->next();
          result_succ != nullptr;
          result = result_succ);
      return result;
    }
  }
}

auto abstract_tree::less_cb(const abstract_tree_elem& x, const abstract_tree_elem& y) const -> bool {
  return less_cb(*x.branch_key_(allocator_type()), *y.branch_key_(allocator_type()));
}

auto abstract_tree::less_cb(const abstract_tree_page_branch_key& x, const abstract_tree_elem& y) const -> bool {
  return less_cb(x, *y.branch_key_(allocator_type()));
}

auto abstract_tree::less_cb(const abstract_tree_elem& x, const abstract_tree_page_branch_key& y) const -> bool {
  return less_cb(*x.branch_key_(allocator_type()), y);
}

void abstract_tree::with_equal_range_for_read(cheap_fn_ref<void(abstract_tree_iterator, abstract_tree_iterator)> cb, const abstract_tree_page_branch_key& key) {
  return with_equal_range_<std::shared_lock<std::shared_mutex>>(std::move(cb), key);
}

void abstract_tree::with_equal_range_for_write(cheap_fn_ref<void(abstract_tree_iterator, abstract_tree_iterator)> cb, const abstract_tree_page_branch_key& key) {
  return with_equal_range_<std::unique_lock<std::shared_mutex>>(std::move(cb), key);
}

void abstract_tree::with_for_each_for_read(cheap_fn_ref<stop_continue(cycle_ptr::cycle_gptr<abstract_tree_elem>)> cb) {
  return with_for_each_<std::shared_lock<std::shared_mutex>>(std::move(cb));
}

void abstract_tree::with_for_each_for_write(cheap_fn_ref<stop_continue(cycle_ptr::cycle_gptr<abstract_tree_elem>)> cb) {
  return with_for_each_<std::unique_lock<std::shared_mutex>>(std::move(cb));
}

void abstract_tree::with_for_each_augment_for_read(
    cheap_fn_ref<bool(const abstract_tree_page_branch_elem&)> filter,
    cheap_fn_ref<stop_continue(cycle_ptr::cycle_gptr<abstract_tree_elem>, cycle_ptr::cycle_gptr<tree_page_leaf>, std::shared_lock<std::shared_mutex>&)> cb) {
  return with_for_each_augment_<std::shared_lock<std::shared_mutex>>(std::move(filter), std::move(cb));
}

void abstract_tree::with_for_each_augment_for_write(
    cheap_fn_ref<bool(const abstract_tree_page_branch_elem&)> filter,
    cheap_fn_ref<stop_continue(cycle_ptr::cycle_gptr<abstract_tree_elem>, cycle_ptr::cycle_gptr<tree_page_leaf>, std::unique_lock<std::shared_mutex>&)> cb) {
  return with_for_each_augment_<std::unique_lock<std::shared_mutex>>(std::move(filter), std::move(cb));
}

auto abstract_tree::ensure_root_page_(txfile& f, allocator_type tx_allocator) -> std::shared_lock<std::shared_mutex> {
  {
    std::shared_lock<std::shared_mutex> lck{ mtx_ };
    if (root_off_ != 0) return lck;
  }

  tx_op_collection ops(tx_allocator);
  auto tx = f.begin(false);
  const std::uint64_t off = allocate_txfile_bytes(tx, tree_page_leaf::encoded_size(*cfg), tx_allocator, ops);

  std::unique_lock<std::shared_mutex> lck(mtx_, std::defer_lock);
  do {
    lck.lock();
    if (root_off_ != 0) {
      lck.unlock();
      std::shared_lock<std::shared_mutex> slck{ mtx_ };
      // We must check again, because another thread could be cleaning up empty pages and zero the pointer.
      if (root_off_ != 0) return slck;
    }
  } while (!lck.owns_lock());

  auto page = cycle_ptr::allocate_cycle<tree_page_leaf>(tx_allocator, shared_from_this(this), tx_allocator);
  page->init_empty(off);
  page->encode(tx);

  tx.commit();
  ops.commit();
  lck.unlock();
  return std::shared_lock<std::shared_mutex>(mtx_);
}

auto abstract_tree::insert_(
    const abstract_tree_page_branch_key& key,
    cheap_fn_ref<cycle_ptr::cycle_gptr<abstract_tree_elem>(allocator_type allocator, cycle_ptr::cycle_gptr<tree_page_leaf>)> elem_constructor,
    allocator_type tx_allocator,
    cycle_ptr::cycle_gptr<abstract_tree_elem> hint)
-> std::pair<cycle_ptr::cycle_gptr<abstract_tree_elem>, bool> {
restart:
  cycle_ptr::cycle_gptr<tree_page_leaf> leaf_page;
  std::unique_lock<std::shared_mutex> leaf_lock;

  if (hint != nullptr) {
    // The choice of page is validated below, so it's not an issue if the page is wrong.
    std::tie(leaf_page, leaf_lock) = hint->lock_parent_for_write();
    if (leaf_page->tree().get() != this)
      throw std::logic_error("tree insertion hint must be part an element from the same tree");
  } else {
    // Start by locking the tree.
    // Since we require a root page, if there isn't one, initialize a new one.
    std::shared_lock<std::shared_mutex> parent_lck = ensure_root_page_(*f_, tx_allocator);

    // Find the page that the insert is to take place in.
    std::tie(leaf_page, leaf_lock) = insert_find_page_(std::move(parent_lck), key);
    assert(leaf_page != nullptr);
    assert(leaf_lock.owns_lock() && leaf_lock.mutex() == &leaf_page->mtx_());
  }

restart_split:
  // If the page has insufficient space, split the page.
  std::tie(leaf_page, leaf_lock) = insert_maybe_split_page_(std::move(leaf_page), std::move(leaf_lock), key, tx_allocator);
  if (leaf_page == nullptr) goto restart; // Restart the loop if we lost track completely.
  assert(leaf_lock.owns_lock() && leaf_lock.mutex() == &leaf_page->mtx_());

  // If the page gets unlocked in any of the steps above,
  // it might have changed the range which it is responsible for.
  // In that case, we might by now be using the wrong page.
  // Therefore we should verify this and restart if we are indeed wrong.
  {
    bool need_restart = false;
    if (less_cb(key, *leaf_page->page_key_)) {
      need_restart = true;
    } else {
      const auto successor_page = leaf_page->next(leaf_lock);
      if (successor_page != nullptr) {
        std::shared_lock<std::shared_mutex> successor_lck{ successor_page->mtx_() };
        if (!less_cb(key, *successor_page->page_key_)) need_restart = true;
      }
    }

    if (need_restart) {
      // If the hint points at this page, it is no longer good.
      if (hint != nullptr) {
        std::shared_lock<std::shared_mutex> hint_lck{ hint->mtx_ref_() };
        if (hint->parent_ == leaf_page) {
          hint_lck.unlock(); // Unlock before potentially deallocating hint.
          hint.reset();
        }
      }
      goto restart;
    }
  }

  // Find the successor of the insert position.
  auto successor_elem = std::find_if(
      leaf_page->elems_.begin(), leaf_page->elems_.end(),
      [this, &key](const auto& elem_ptr) -> bool {
        if (elem_ptr == nullptr) return false;
        return !less_cb(key, *elem_ptr);
      });
  // If the element with this key already exists, fail the insertion.
  if (successor_elem != leaf_page->elems_.end()
      && !less_cb(**successor_elem, key)) {
    return std::make_pair(*successor_elem, false);
  }

  {
    // See if we can move a null forward.
    auto null_iter = std::find(std::next(successor_elem), leaf_page->elems_.end(), nullptr);
    if (null_iter == leaf_page->elems_.end()) goto end_move_a_null_forward;

    auto shift_tx = f_->begin(false);
    tx_op_collection shift_ops(tx_allocator);

    // Shift the elements one position.
    std::copy_backward(successor_elem, null_iter, std::next(null_iter));
    successor_elem->reset();
    shift_ops.on_rollback(
        [successor_elem, null_iter]() {
          std::copy(std::next(successor_elem), std::next(null_iter), successor_elem);
          null_iter->reset();
        });
    auto ins_pos = successor_elem;
    ++successor_elem;

    // Insert the element.
    *ins_pos = elem_constructor(leaf_page->allocator, leaf_page);
    if (*ins_pos == nullptr) throw std::logic_error("tree: null insertion");
    assert(!less_cb(key, **ins_pos) && !less_cb(**ins_pos, key) && "key compares the same as constructed element");

    // Update on-disk representation.
    leaf_page->encode(shift_tx);

    shift_tx.commit();
    shift_ops.commit(); // Never throws.

    // XXX we ought to update the augmentations.

    return std::make_pair(*ins_pos, true);
  }
end_move_a_null_forward:

  // Taking a null from after the insert pos didn't work (tried and failed that above).
  // And there are no elements in front of the successor.
  // That can only be the case if the page is full.
  if (successor_elem == leaf_page->elems_.begin())
    goto restart_split;

  {
    // Move a null from before the insert position to here.
    auto ins_pos = std::prev(successor_elem);
    auto null_iter = ins_pos;
    while (null_iter != leaf_page->elems_.begin() && *null_iter != nullptr)
      --null_iter;
    // Apparently the page has no space available...
    if (*null_iter != nullptr) goto restart_split;

    auto shift_tx = f_->begin(false);
    tx_op_collection shift_ops(tx_allocator);

    // Shift the elements back one position.
    std::copy(std::next(null_iter), successor_elem, null_iter);
    ins_pos->reset();
    shift_ops.on_rollback(
        [null_iter, ins_pos, successor_elem]() {
          std::copy_backward(null_iter, ins_pos, successor_elem);
          null_iter->reset();
        });

    // Insert the element.
    *ins_pos = elem_constructor(leaf_page->allocator, leaf_page);
    if (*ins_pos == nullptr) throw std::logic_error("tree: null insertion");
    assert(!less_cb(key, **ins_pos) && !less_cb(**ins_pos, key) && "key compares the same as constructed element");

    // Update on-disk representation.
    leaf_page->encode(shift_tx);

    shift_tx.commit();
    shift_ops.commit(); // Never throws.

    // XXX we ought to update the augmentations.

    return std::make_pair(*ins_pos, true);
  }
}

auto abstract_tree::insert_find_page_(std::shared_lock<std::shared_mutex>&& lck, const abstract_tree_page_branch_key& key)
-> std::pair<cycle_ptr::cycle_gptr<tree_page_leaf>, std::unique_lock<std::shared_mutex>> {
  assert(lck.owns_lock() && lck.mutex() == &mtx_);
  assert(root_off_ != 0);

  cycle_ptr::cycle_gptr<tree_page_branch> parent_page;
  std::shared_lock<std::shared_mutex> parent_lck = std::move(lck);

  cycle_ptr::cycle_gptr<abstract_tree_page> child_page = get(root_off_);
  while (auto branch_page = std::dynamic_pointer_cast<tree_page_branch>(child_page)) {
    parent_page = branch_page;
    parent_lck = std::shared_lock<std::shared_mutex>{ branch_page->mtx_() };
    const auto i = std::lower_bound(branch_page->keys_.begin(), branch_page->keys_.end(), &key,
        [this](const auto& x, const auto& y) -> bool {
          return less_cb(*x, *y);
        });
    child_page = get(branch_page->elems_[i - branch_page->keys_.begin()]->off);
  }

  cycle_ptr::cycle_gptr<tree_page_leaf> leaf_page = boost::polymorphic_pointer_downcast<tree_page_leaf>(child_page);
  std::unique_lock<std::shared_mutex> leaf_lock{ leaf_page->mtx_() };
  return std::make_pair(std::move(leaf_page), std::move(leaf_lock));
}

auto abstract_tree::insert_maybe_split_page_(
    cycle_ptr::cycle_gptr<tree_page_leaf> leaf_page,
    std::unique_lock<std::shared_mutex> leaf_lck,
    const abstract_tree_page_branch_key& key,
    allocator_type tx_allocator)
-> std::pair<cycle_ptr::cycle_gptr<tree_page_leaf>, std::unique_lock<std::shared_mutex>> {
  assert(leaf_lck.owns_lock() && leaf_lck.mutex() == &leaf_page->mtx_());

  // If the leaf page has space, return immediately.
  if (leaf_page->elems_.size() < leaf_page->elems_.capacity()
      || std::find_if(leaf_page->elems_.cbegin(), leaf_page->elems_.cend(), [](const auto& ptr) { return ptr == nullptr; }) != leaf_page->elems_.end()) {
    return std::forward_as_tuple(std::move(leaf_page), std::move(leaf_lck));
  }

  cycle_ptr::cycle_gptr<tree_page_branch> parent_page;
  std::unique_lock<std::shared_mutex> parent_lck;

  // If there is no parent page, construct one.
  if (leaf_page->parent_off_ == 0) {
    leaf_lck.unlock();

    auto tx = f_->begin(false);
    tx_op_collection ops(tx_allocator);
    const std::uint64_t new_parent_off = allocate_txfile_bytes(tx, tree_page_branch::encoded_size(*cfg), tx_allocator, ops);

    std::unique_lock tree_lck{ mtx_ };
    leaf_lck.lock();
    if (leaf_page->parent_off_ != 0) goto end_parent_off_is_zero;

    assert(root_off_ == leaf_page->off_);
    ops.on_commit(
        [this, new_parent_off]() {
          root_off_ = new_parent_off;
        });
    ops.on_commit(
        [leaf_page, new_parent_off]() {
          leaf_page->parent_off_ = new_parent_off;
        });

    parent_page = boost::polymorphic_pointer_downcast<tree_page_branch>(
        db_cache_->create(
            new_parent_off, shared_from_this(this),
            [this, &leaf_page, &leaf_lck](auto alloc, auto offset) {
              cycle_ptr::cycle_gptr<tree_page_branch> branch = allocate_branch_(alloc);
              branch->init(offset, leaf_page, leaf_lck);
              return branch;
            },
            ops));

    parent_lck = std::unique_lock<std::shared_mutex>(parent_page->mtx_());

    tx.commit(); // Write new state to disk.
    ops.commit(); // Never throws.
  }
end_parent_off_is_zero:
  assert(leaf_page->parent_off_ != 0);

  if (parent_page == nullptr) { // Need to get and lock the parent.
    do {
      parent_page = boost::polymorphic_pointer_downcast<tree_page_branch>(get(leaf_page->parent_off_));
      parent_lck = std::unique_lock(parent_page->mtx_(), std::try_to_lock); // Try-lock, because we go against lock ordering.
      if (!parent_lck.owns_lock()) {
        leaf_lck.unlock();
        parent_lck.lock();
        leaf_lck.lock();

        if (parent_page->off_ != leaf_page->parent_off_)
          parent_lck.unlock(); // Wrong parent.
      }
    } while (!parent_lck.owns_lock());
  }
  assert(parent_page != nullptr);
  assert(parent_lck.owns_lock() && parent_lck.mutex() == &parent_page->mtx_());
  assert(leaf_lck.owns_lock() && leaf_lck.mutex() == &leaf_page->mtx_());
  assert(parent_page->off_ == leaf_page->parent_off_);

  leaf_lck.unlock();
  parent_page = insert_maybe_split_page_parents_(
      leaf_page,
      std::move(parent_page),
      std::move(parent_lck),
      tx_allocator);
  if (parent_page == nullptr) // We lost track.
    return std::make_pair(nullptr, std::unique_lock<std::shared_mutex>());

  // While we don't have the lock, start preparation for the expected page-split.
  auto tx = f_->begin(false);
  // Start by allocating space.
  tx_op_collection ops(tx_allocator);
  const std::uint64_t new_page_off = allocate_txfile_bytes(tx, tree_page_leaf::encoded_size(*cfg), tx_allocator, ops);

  // Now acquire the lock.
  parent_lck.lock();
  assert(parent_lck.owns_lock() && parent_lck.mutex() == &parent_page->mtx_());
  leaf_lck.lock();
  assert(leaf_lck.owns_lock() && leaf_lck.mutex() == &leaf_page->mtx_());

  /*
   * Perform revalidation.
   * We bail out if:
   * - we no longer got our parent/child relationship correct (unsuccessful bailout)
   * - the leaf has been granted space during our unlocks (successful bailout)
   */
  if (parent_page->off_ != leaf_page->parent_off_) // We lost track.
    return std::make_pair(nullptr, std::unique_lock<std::shared_mutex>());
  // If the leaf page has acquired space during any of the unlocks, return immediately.
  if (leaf_page->elems_.size() < leaf_page->elems_.capacity()
      || std::find_if(leaf_page->elems_.cbegin(), leaf_page->elems_.cend(), [](const auto& ptr) { return ptr == nullptr; }) != leaf_page->elems_.end()) {
    return std::forward_as_tuple(std::move(leaf_page), std::move(leaf_lck));
  }

  // Split the leaf page, moving approximately half its elements into the sibling.
  std::shared_ptr<abstract_tree_page_branch_key> split_key;
  cycle_ptr::cycle_gptr<tree_page_leaf> sibling_page;
  cycle_ptr::cycle_gptr<void> sibling_lock_insurance; // Used to maintain a reference to the item locked by sibling_lock.
  std::unique_lock<std::shared_mutex> sibling_lock;
  sibling_page = boost::polymorphic_pointer_downcast<tree_page_leaf>(
      db_cache_->create( // Do this via the cache, so the cache gets populated with the new page.
          new_page_off, shared_from_this(this),
          [&](auto alloc, auto off) {
            cycle_ptr::cycle_gptr<tree_page_leaf> sp;
            std::tie(split_key, sp, sibling_lock) = leaf_page->local_split_(
                leaf_lck, tx, off,
                parent_page, parent_lck,
                alloc, tx_allocator, ops);
            sibling_lock_insurance = sp;
            return sp;
          },
          ops));

  tx.commit();
  ops.commit(); // Never throws.

  if (less_cb(key, *split_key))
    return std::make_pair(std::move(leaf_page), std::move(leaf_lck));
  else
    return std::make_pair(std::move(sibling_page), std::move(sibling_lock));
}

auto abstract_tree::insert_maybe_split_page_parents_(
    cycle_ptr::cycle_gptr<tree_page_leaf> leaf_page,
    cycle_ptr::cycle_gptr<tree_page_branch> parent_page,
    std::unique_lock<std::shared_mutex> parent_lck,
    allocator_type tx_allocator)
-> cycle_ptr::cycle_gptr<tree_page_branch> {
  using stack_t = std::stack<
      cycle_ptr::cycle_gptr<tree_page_branch>,
      std::deque<cycle_ptr::cycle_gptr<tree_page_branch>, traits_type::rebind_alloc<cycle_ptr::cycle_gptr<tree_page_branch>>>>;

  assert(parent_page != nullptr);
  assert(parent_lck.owns_lock() && parent_lck.mutex() == &parent_page->mtx_());

  // If there is space, we have nothing to do here.
  if (parent_page->elems_.size() < parent_page->elems_.capacity())
    return parent_page;

  cycle_ptr::cycle_gptr<tree_page_branch> page = std::move(parent_page);
  std::unique_lock<std::shared_mutex> lck = std::move(parent_lck);

  stack_t stack;
  while (page != nullptr && page->elems_.size() == page->elems_.capacity()) {
    std::unique_lock<std::shared_mutex> stack_elem_lck = std::move(lck);
    stack.push(std::move(page));
    do {
      if (stack.top()->parent_off_ == 0) {
        page.reset();
        lck = std::unique_lock<std::shared_mutex>(mtx_, std::try_to_lock); // Try-lock, because we are going against the lock ordering.
      } else {
        page = boost::polymorphic_pointer_downcast<tree_page_branch>(get(stack.top()->parent_off_));
        lck = std::unique_lock<std::shared_mutex>(page->mtx_(), std::try_to_lock); // Try-lock, because we are going against the lock ordering.
      }

      if (!lck.owns_lock()) {
        // Lock the parent and the current top-of-stack, while respecting lock ordering.
        stack_elem_lck.unlock();
        lck.lock();
        stack_elem_lck.lock();

        if (page == nullptr) {
          if (page->parent_off_ != 0) lck.unlock();
        } else {
          // Reject if the offsets don't match (which can happen if this page got shifted around).
          if (page->off_ != stack.top()->parent_off_) lck.unlock();
        }
      }
    } while (!lck.owns_lock());
  }

  if (page == nullptr) { // We need to create a new level in the tree.
    assert(lck.owns_lock() && lck.mutex() == &mtx_);
    std::unique_lock<std::shared_mutex> stack_elem_lck{ stack.top()->mtx_() };

    // If things shift around too much, we get lost.
    // In that case, bail out.
    if (stack.top()->parent_off_ != 0) return nullptr;
    assert(root_off_ == stack.top()->off_);

    // Allocate a page while not under a lock.
    stack_elem_lck.unlock();
    lck.unlock();
    auto tx = f_->begin(false);
    tx_op_collection ops(tx_allocator);
    const std::uint64_t new_parent_off = allocate_txfile_bytes(tx, tree_page_branch::encoded_size(*cfg), tx_allocator, ops);

    lck.lock();
    stack_elem_lck.lock();

    // If things got changed while we were unlocked:
    // bail out.
    if (stack.top()->parent_off_ != 0
        || root_off_ != stack.top()->off_)
      return nullptr;

    ops.on_commit(
        [self=shared_from_this(this), new_parent_off]() {
          self->root_off_ = new_parent_off;
        });
    ops.on_commit(
        [stack_page=stack.top(), new_parent_off]() {
          stack_page->parent_off_ = new_parent_off;
        });

    page = boost::polymorphic_pointer_downcast<tree_page_branch>(
        db_cache_->create(
            new_parent_off, shared_from_this(this),
            [&stack, &stack_elem_lck, this](auto alloc, auto offset) {
              cycle_ptr::cycle_gptr<tree_page_branch> branch = allocate_branch_(alloc);
              branch->init(offset, stack.top(), stack_elem_lck);
              return branch;
            },
            ops));

    std::unique_lock<std::shared_mutex> new_parent_lck{ parent_page->mtx_() };

    tx.commit();
    ops.commit(); // Never throws.

    lck = std::move(new_parent_lck);
  }
  assert(page != nullptr);
  assert(lck.owns_lock() && lck.mutex() == &page->mtx_());
  assert(page->elems_.size() < page->elems_.capacity());

  while (!stack.empty()) {
    // Allocate space in the file for a new branch page.
    lck.unlock();
    auto tx = f_->begin(false);
    tx_op_collection ops(tx_allocator);
    const std::uint64_t new_page_off = allocate_txfile_bytes(tx, tree_page_branch::encoded_size(*cfg), tx_allocator, ops);

    auto& stack_elem = stack.top();
    lck.lock();
    std::unique_lock<std::shared_mutex> stack_elem_lck{ stack_elem->mtx_() };

    // If things shift around too much, we get lost.
    // In that case, bail out.
    if (stack_elem->parent_off_ != page->off_) return nullptr;

    // Split the selected branch page, moving approximately half its elements into the sibling.
    std::shared_ptr<abstract_tree_page_branch_key> split_key;
    cycle_ptr::cycle_gptr<tree_page_branch> sibling_page;
    cycle_ptr::cycle_gptr<void> sibling_lock_insurance; // Used to maintain a reference to the item locked by sibling_lock.
    std::unique_lock<std::shared_mutex> sibling_lock;
    sibling_page = boost::polymorphic_pointer_downcast<tree_page_branch>(
        db_cache_->create( // Do this via the cache, so the cache gets populated with the new page.
            new_page_off, shared_from_this(this),
            [&](auto alloc, auto off) {
              cycle_ptr::cycle_gptr<tree_page_branch> sp;
              std::tie(split_key, sp, sibling_lock) = stack_elem->local_split_(
                  stack_elem_lck, tx, off,
                  page, lck,
                  alloc, tx_allocator, ops);
              sibling_lock_insurance = sp;
              return sp;
            },
            ops));

    tx.commit();
    ops.commit(); // Never throws.

    // Use a read lock if we unlocked the leaf element, so we can read the page-key.
    std::shared_lock<std::shared_mutex> leaf_page_backup_lck{ leaf_page->mtx_() };

    if (less_cb(*leaf_page->page_key_, *split_key)) {
      lck = std::move(stack_elem_lck);
      page = std::move(stack_elem);
    } else {
      lck = std::move(sibling_lock);
      page = std::move(sibling_page);
    }
    stack.pop();
  }

  return page;
}


abstract_tree_page::~abstract_tree_page() noexcept = default;

auto abstract_tree_page::tree() const -> cycle_ptr::cycle_gptr<abstract_tree> {
  return cycle_ptr::cycle_gptr<abstract_tree>(tree_);
}

auto abstract_tree_page::decode(
    abstract_tree& tree,
    const txfile::transaction& tx, std::uint64_t off,
    abstract_tree::allocator_type allocator)
-> cycle_ptr::cycle_gptr<abstract_tree_page> {
  std::uint32_t magic;
  monsoon::io::read_at(tx, off, &magic, sizeof(magic));
  boost::endian::big_to_native_inplace(magic);

  cycle_ptr::cycle_gptr<abstract_tree_page> page;
  switch (magic) {
    default:
      throw tree_error("bad tree page magic");
    case tree_page_leaf::magic:
      page = tree.allocate_leaf_(std::move(allocator));
      break;
    case tree_page_branch::magic:
      page = tree.allocate_branch_(std::move(allocator));
      break;
  }
  page->decode(tx, off);
  return page;
}

void abstract_tree_page::reparent_([[maybe_unused]] std::uint64_t old_parent_off, std::uint64_t new_parent_off) noexcept {
  std::lock_guard<std::shared_mutex> lck{ mtx_() };
  assert(parent_off_ == old_parent_off || parent_off_ == new_parent_off); // We may have been loaded before or after the reparenting process started.
  parent_off_ = new_parent_off;
}

auto abstract_tree_page::local_split_(
    const std::unique_lock<std::shared_mutex>& lck, txfile::transaction& tx, std::uint64_t new_page_off,
    cycle_ptr::cycle_gptr<tree_page_branch> parent, const std::unique_lock<std::shared_mutex>& parent_lck,
    abstract_tree::allocator_type sibling_allocator,
    abstract_tree::allocator_type tx_allocator,
    tx_op_collection& ops)
-> std::tuple<
    std::shared_ptr<abstract_tree_page_branch_key>,
    cycle_ptr::cycle_gptr<abstract_tree_page>,
    std::unique_lock<std::shared_mutex>
> {
  return local_split_atp_(lck, tx, new_page_off, std::move(parent), parent_lck, std::move(sibling_allocator), std::move(tx_allocator), ops);
}


auto tree_page_leaf::encoded_size(const tree_cfg& cfg) -> std::size_t {
  // Items:
  // - 1 header
  // - 1 key
  // - N values
  return header::SIZE + cfg.key_bytes + cfg.items_per_leaf_page * cfg.val_bytes;
}

tree_page_leaf::tree_page_leaf(cycle_ptr::cycle_gptr<abstract_tree> tree, abstract_tree::allocator_type allocator)
: abstract_tree_page(std::move(tree), std::move(allocator)),
  elems_(elems_vector::allocator_type(*this, this->allocator))
{
  elems_.reserve(cfg->items_per_leaf_page);
}

tree_page_leaf::~tree_page_leaf() noexcept = default;

void tree_page_leaf::init_empty(std::uint64_t off) {
  assert(elems_.empty());

  off_ = off;
  elems_.resize(cfg->items_per_leaf_page);
}

void tree_page_leaf::decode(const txfile::transaction& tx, std::uint64_t off) {
  assert(elems_.empty());

  const std::size_t bytes_per_val = cfg->val_bytes;

  const auto t = tree();
  auto buf_storage = std::vector<std::uint8_t>(encoded_size(*cfg));
  monsoon::io::read_at(tx, off, buf_storage.data(), buf_storage.size());
  boost::asio::const_buffer buf = boost::asio::buffer(buf_storage);

  header h;
  h.decode(buf);
  buf += header::SIZE;

  if ((h.flags & header::flag_has_key) == 0) {
    page_key_.reset();
  } else {
    page_key_ = t->allocate_branch_key_(allocator);
    assert(buf.size() >= cfg->key_bytes);
    page_key_->decode(boost::asio::buffer(buf.data(), cfg->key_bytes));
  }
  buf += cfg->key_bytes;

  off_ = off;
  parent_off_ = h.parent_off;
  next_sibling_off_ = h.next_sibling_off;
  prev_sibling_off_ = h.prev_sibling_off;
  std::generate_n(
      std::back_inserter(elems_),
      cfg->items_per_leaf_page,
      [&buf, bytes_per_val, this, &t]() -> cycle_ptr::cycle_gptr<abstract_tree_elem> {
        assert(buf.size() >= bytes_per_val);

        auto e = t->allocate_elem_(this->shared_from_this(this), allocator);
        e->decode(boost::asio::buffer(buf.data(), bytes_per_val));
        buf += bytes_per_val;
        if (e->is_never_visible()) e.reset();
        return e;
      });

  assert(buf.size() == 0); // Buffer should have been fully consumed.

  // Fix up the predecessor and successor pointers of the elements.
  const auto find_not_null = [this](elems_vector::iterator iter) -> elems_vector::iterator {
    return std::find_if(
        iter, elems_.end(),
        [](const auto& ptr) noexcept -> bool { return ptr != nullptr; });
  };
  auto pred = find_not_null(elems_.begin());
  if (pred != elems_.end()) {
    for (auto succ = find_not_null(std::next(pred));
        succ != elems_.end();
        pred = std::exchange(succ, find_not_null(std::next(succ)))) {
      (*succ)->pred_ = *pred;
      (*pred)->succ_ = *succ;
    }
  }
}

void tree_page_leaf::encode(txfile::transaction& tx) const {
  assert(!elems_.empty());
  assert(elems_.size() == cfg->items_per_leaf_page);

  const std::size_t bytes_per_val = cfg->val_bytes;

  auto buf_storage = std::vector<std::uint8_t>(encoded_size(*cfg));
  boost::asio::mutable_buffer buf = boost::asio::buffer(buf_storage);

  header h;
  h.magic = magic;
  h.flags = 0;
  if (page_key_ != nullptr) h.flags |= header::flag_has_key;
  h.parent_off = parent_off_;
  h.next_sibling_off = next_sibling_off_;
  h.prev_sibling_off = prev_sibling_off_;
  h.encode(buf);
  buf += header::SIZE;

  if (page_key_ != nullptr)
    page_key_->encode(buf);
  else
    std::memset(buf.data(), 0, (buf.size() < cfg->key_bytes ? buf.size() : cfg->key_bytes));
  buf += cfg->key_bytes;

  std::for_each(
      elems_.begin(), elems_.end(),
      [&buf, bytes_per_val](const auto& elem_ptr) {
        assert(buf.size() >= bytes_per_val);

        boost::asio::mutable_buffer elem_buf = boost::asio::buffer(buf.data(), bytes_per_val);
        buf += bytes_per_val;

        if (elem_ptr == nullptr)
          std::memset(elem_buf.data(), 0, elem_buf.size());
        else
          elem_ptr->encode(elem_buf);
      });

  assert(buf.size() == 0); // Buffer should have been fully filled.
  monsoon::io::write_at(tx, off_, buf_storage.data(), buf_storage.size());
}

auto tree_page_leaf::next() const -> cycle_ptr::cycle_gptr<tree_page_leaf> {
  std::shared_lock<std::shared_mutex> lck{ mtx_() };
  return next(lck);
}

auto tree_page_leaf::prev() const -> cycle_ptr::cycle_gptr<tree_page_leaf> {
  std::shared_lock<std::shared_mutex> lck{ mtx_() };
  return prev(lck);
}

auto tree_page_leaf::next(const std::shared_lock<std::shared_mutex>& lck) const -> cycle_ptr::cycle_gptr<tree_page_leaf> {
  assert(lck.owns_lock() && lck.mutex() == &mtx_());

  if (next_sibling_off_ == 0) return nullptr;
  return boost::polymorphic_pointer_downcast<tree_page_leaf>(tree()->get(next_sibling_off_));
}

auto tree_page_leaf::prev(const std::shared_lock<std::shared_mutex>& lck) const -> cycle_ptr::cycle_gptr<tree_page_leaf> {
  assert(lck.owns_lock() && lck.mutex() == &mtx_());

  if (prev_sibling_off_ == 0) return nullptr;
  return boost::polymorphic_pointer_downcast<tree_page_leaf>(tree()->get(prev_sibling_off_));
}

auto tree_page_leaf::next(const std::unique_lock<std::shared_mutex>& lck) const -> cycle_ptr::cycle_gptr<tree_page_leaf> {
  assert(lck.owns_lock() && lck.mutex() == &mtx_());

  if (next_sibling_off_ == 0) return nullptr;
  return boost::polymorphic_pointer_downcast<tree_page_leaf>(tree()->get(next_sibling_off_));
}

auto tree_page_leaf::prev(const std::unique_lock<std::shared_mutex>& lck) const -> cycle_ptr::cycle_gptr<tree_page_leaf> {
  assert(lck.owns_lock() && lck.mutex() == &mtx_());

  if (prev_sibling_off_ == 0) return nullptr;
  return boost::polymorphic_pointer_downcast<tree_page_leaf>(tree()->get(prev_sibling_off_));
}

auto tree_page_leaf::compute_augment(const std::shared_lock<std::shared_mutex>& lck, abstract_tree::allocator_type allocator) const -> std::shared_ptr<abstract_tree_page_branch_elem> {
  assert(lck.owns_lock() && lck.mutex() == &mtx_());
  return tree()->compute_augment_(off_, { elems_.begin(), elems_.end() }, std::move(allocator));
}

auto tree_page_leaf::compute_augment(const std::unique_lock<std::shared_mutex>& lck, abstract_tree::allocator_type allocator) const -> std::shared_ptr<abstract_tree_page_branch_elem> {
  assert(lck.owns_lock() && lck.mutex() == &mtx_());
  return tree()->compute_augment_(off_, { elems_.begin(), elems_.end() }, std::move(allocator));
}

auto tree_page_leaf::local_split_(
    const std::unique_lock<std::shared_mutex>& lck, txfile::transaction& tx, std::uint64_t new_page_off,
    cycle_ptr::cycle_gptr<tree_page_branch> parent, const std::unique_lock<std::shared_mutex>& parent_lck,
    abstract_tree::allocator_type sibling_allocator,
    abstract_tree::allocator_type tx_allocator,
    tx_op_collection& ops)
-> std::tuple<
    std::shared_ptr<abstract_tree_page_branch_key>,
    cycle_ptr::cycle_gptr<tree_page_leaf>,
    std::unique_lock<std::shared_mutex>
> {
  assert(parent != nullptr);
  assert(parent->offset() == parent_off_);
  assert(lck.owns_lock() && is_my_mutex(lck.mutex()));
  assert(parent_lck.owns_lock() && parent->is_my_mutex(parent_lck.mutex()));

  const auto t = tree();

  // Allocate all objects and figure out parameters.
  const elems_vector::iterator sibling_begin = split_select_(lck);
  cycle_ptr::cycle_gptr<tree_page_leaf> sibling = t->allocate_leaf_(sibling_allocator);
  std::unique_lock<std::shared_mutex> sibling_lck{ sibling->mtx_() };
  std::shared_ptr<abstract_tree_page_branch_key> sibling_key = (*sibling_begin)->branch_key_(parent->allocator);

  // Initialize sibling.
  sibling->init_empty(new_page_off);
  sibling->prev_sibling_off_ = off_;
  sibling->next_sibling_off_ = next_sibling_off_;
  sibling->parent_off_ = parent_off_;
  sibling->page_key_ = (*sibling_begin)->branch_key_(sibling_allocator);
  std::copy(
      sibling_begin, elems_.end(),
      sibling->elems_.begin());
  sibling->encode(tx);

  // Change predecessor pointer of current successor to point at sibling.
  if (next_sibling_off_ != 0) {
    header h;
    h.prev_sibling_off = new_page_off;
    h.native_to_big_endian();
    monsoon::io::write_at(tx, next_sibling_off_ + offsetof(header, prev_sibling_off), &h.prev_sibling_off, sizeof(h.prev_sibling_off));
  }

  // Invalidate our moved elements on disk (but not yet in memory).
  const std::uint64_t zero_begin = offset_for_idx_(sibling_begin - elems_.begin());
  const std::uint64_t zero_end = offset_for_idx_(elems_.size());
  std::vector<std::uint8_t> tmp(zero_end - zero_begin);
  std::memset(tmp.data(), 0, tmp.size());
  monsoon::io::write_at(tx, zero_begin, tmp.data(), tmp.size());

  // Compute augments for this and sibling page.
  std::shared_ptr<abstract_tree_page_branch_elem> this_augment = t->compute_augment_(off_, { elems_.begin(), sibling_begin }, parent->allocator);
  std::shared_ptr<abstract_tree_page_branch_elem> sibling_augment = t->compute_augment_(new_page_off, { sibling_begin, elems_.end() }, parent->allocator);

  // On commit:
  // Break the pred/succ link in the elements.
  ops.on_commit(
      [elem_after_split=cycle_ptr::cycle_gptr<abstract_tree_elem>(*sibling_begin)]() {
        std::lock_guard<std::shared_mutex> elem_lck{ elem_after_split->mtx_ref_() };
        elem_after_split->pred_.reset();
      });
  ops.on_commit(
      [elem_before_split=cycle_ptr::cycle_gptr<abstract_tree_elem>(*std::prev(sibling_begin))]() {
        std::lock_guard<std::shared_mutex> elem_lck{ elem_before_split->mtx_ref_() };
        elem_before_split->succ_.reset();
      });

  // On commit:
  // Update predecessor offset if (old) successor is loaded in memory.
  // And update our sibling reference.
  ops.on_commit(
      [self=shared_from_this(this), new_page_off, t]() {
        const auto old_successor = boost::polymorphic_pointer_downcast<tree_page_leaf>(t->get_if_present(self->next_sibling_off_));
        if (old_successor != nullptr) {
          std::lock_guard<std::shared_mutex> old_successor_lck{ old_successor->mtx_() };
          assert(old_successor->prev_sibling_off_ == self->off_ || old_successor->prev_sibling_off_ == new_page_off);
          old_successor->prev_sibling_off_ = new_page_off;
        }

        self->next_sibling_off_ = new_page_off;
      });

  // On commit:
  // Update parent-pointer of elements given to sibling.
  // Release elements we gave to sibling.
  ops.on_commit(
      [self=shared_from_this(this), sibling_begin, new_page_off, sibling]() {
        // Update parent-pointer of elements given to sibling.
        std::for_each(
            sibling_begin, self->elems_.end(),
            [new_page_off, &sibling](elems_vector::const_reference elem_ptr) {
              std::lock_guard<std::shared_mutex> elem_lck{ elem_ptr->mtx_ref_() };
              elem_ptr->parent_ = sibling;
            });

        // Release elements we gave to sibling.
        std::fill(sibling_begin, self->elems_.end(), nullptr);
      });

  // Update parent page to have the sibling present.
  parent->insert_sibling(parent_lck, tx, *this, std::move(this_augment), *sibling, sibling_key, std::move(sibling_augment), tx_allocator, ops);

  tx.commit();
  return std::forward_as_tuple(
      std::move(sibling_key),
      std::move(sibling),
      std::move(sibling_lck));
}

auto tree_page_leaf::local_split_atp_(
    const std::unique_lock<std::shared_mutex>& lck, txfile::transaction& tx, std::uint64_t new_page_off,
    cycle_ptr::cycle_gptr<tree_page_branch> parent, const std::unique_lock<std::shared_mutex>& parent_lck,
    abstract_tree::allocator_type sibling_allocator,
    abstract_tree::allocator_type tx_allocator,
    tx_op_collection& ops)
-> std::tuple<
    std::shared_ptr<abstract_tree_page_branch_key>,
    cycle_ptr::cycle_gptr<abstract_tree_page>,
    std::unique_lock<std::shared_mutex>
> {
  return local_split_(lck, tx, new_page_off, std::move(parent), parent_lck, std::move(sibling_allocator), std::move(tx_allocator), ops);
}

auto tree_page_leaf::split_select_(const std::unique_lock<std::shared_mutex>& lck) -> elems_vector::iterator {
  assert(lck.owns_lock() && lck.mutex() == &mtx_());

  struct pred_non_null_elem {
    auto operator()(elems_vector::const_reference elem_ptr) const noexcept -> bool {
      return elem_ptr != nullptr;
    }
  };

  // Pick sibling_begin such that it is approximately halfway the page and is not null.
  elems_vector::iterator sibling_begin = elems_.begin() + elems_.size() / 2u;

  // If there are no non-null elements preceding sibling_begin, search forward.
  if (std::none_of(elems_.begin(), sibling_begin, pred_non_null_elem())) {
    sibling_begin = std::find_if(sibling_begin, elems_.end(), pred_non_null_elem());
    if (sibling_begin == elems_.end()) throw std::logic_error("cannot split empty page");
    ++sibling_begin;
  }

  // Ensure sibling_begin points at a non-null element.
  sibling_begin = std::find_if(sibling_begin, elems_.end(), pred_non_null_elem());
  if (sibling_begin == elems_.end()) throw std::logic_error("cannot split page with only 1 element");

  // Validate post-conditions.
  assert(sibling_begin != elems_.end()); // sibling_begin is dereferencable
  assert(*sibling_begin != nullptr); // sibling_begin is not a nullptr
  assert(std::any_of(elems_.begin(), sibling_begin, pred_non_null_elem())); // Retained range contains at least 1 element.
  assert(std::any_of(sibling_begin, elems_.end(), pred_non_null_elem())); // Suffix range contains at least 1 element.
  return sibling_begin;
}

auto tree_page_leaf::offset_for_idx_(elems_vector::size_type idx) const noexcept -> std::uint64_t {
  assert(idx <= cfg->items_per_leaf_page); // We allow the "end" index.

  const std::size_t bytes_per_val = tx_aware_data::TX_AWARE_SIZE + cfg->key_bytes + cfg->val_bytes;
  const auto rel_off = header::SIZE + cfg->key_bytes + idx * bytes_per_val;
  return off_ + rel_off;
}

auto tree_page_leaf::offset_for_(const abstract_tree_elem& elem) const noexcept -> std::uint64_t {
  const auto pos = std::find_if(
      elems_.begin(), elems_.end(),
      [&elem](elems_vector::const_reference ptr) -> bool {
        return ptr.get() == &elem;
      });
  assert(pos != elems_.end());
  return offset_for_idx_(pos - elems_.begin());
}

auto tree_page_leaf::mtx_() const noexcept -> std::shared_mutex& {
  return layout_mtx;
}

auto tree_page_leaf::get_layout_domain() const noexcept -> const layout_domain& {
  class layout_domain_impl final
  : public layout_domain
  {
    public:
    ~layout_domain_impl() noexcept override = default;

    auto less_compare(const layout_obj& x, const layout_obj& y) const -> bool override {
      return less_compare_(as_leaf_(x), as_leaf_(y));
    }

    private:
    static auto as_leaf_(const layout_obj& lobj) noexcept -> const tree_page_leaf& {
      return *boost::polymorphic_downcast<const tree_page_leaf*>(&lobj);
    }

    static auto first_elem_(const tree_page_leaf& page) noexcept -> const abstract_tree_page_branch_key* {
      return page.page_key_.get();
    }

    static auto less_compare_(const tree_page_leaf& x, const tree_page_leaf& y) -> bool {
      const cycle_ptr::cycle_gptr<const abstract_tree> x_tree = x.tree();
      const cycle_ptr::cycle_gptr<const abstract_tree> y_tree = y.tree();
      if (x_tree != y_tree) return x_tree < y_tree;
      return less_compare_(*x_tree, first_elem_(x), first_elem_(y));
    }

    static auto less_compare_(const abstract_tree& tree, const abstract_tree_page_branch_key* x, const abstract_tree_page_branch_key* y) -> bool {
      if (x == nullptr || y == nullptr) return x < y; // nullptr indicates the very first page.
      return tree.less_cb(*x, *y);
    }
  };

  static const layout_domain_impl impl;
  return impl;
}


auto tree_page_branch::encoded_size(const tree_cfg& cfg) -> std::size_t {
  const std::size_t bytes_per_elem = abstract_tree_page_branch_elem::offset_size + cfg.augment_bytes;

  // Items:
  // - 1 header
  // - N-1 keys
  // - N elems (augments + offset)
  return header::SIZE
      + (cfg.items_per_node_page - 1u) * cfg.key_bytes
      + cfg.items_per_node_page * bytes_per_elem;
}

tree_page_branch::tree_page_branch(cycle_ptr::cycle_gptr<abstract_tree> tree, abstract_tree::allocator_type allocator)
: abstract_tree_page(std::move(tree), std::move(allocator)),
  elems_(this->allocator),
  keys_(this->allocator)
{
  elems_.reserve(cfg->items_per_node_page);
  keys_.reserve(cfg->items_per_node_page - 1u);
}

tree_page_branch::~tree_page_branch() noexcept = default;

void tree_page_branch::init(std::uint64_t off, cycle_ptr::cycle_gptr<abstract_tree_page> elem0, const std::unique_lock<std::shared_mutex>& elem0_lck) {
  elems_.push_back(elem0->compute_augment(elem0_lck, allocator));
  off_ = off;
}

void tree_page_branch::decode(const txfile::transaction& tx, std::uint64_t off) {
  assert(elems_.empty());

  const std::size_t bytes_per_elem = abstract_tree_page_branch_elem::offset_size + cfg->augment_bytes;
  const std::size_t bytes_per_key = cfg->key_bytes;

  auto buf_storage = std::vector<std::uint8_t>(encoded_size(*cfg));
  monsoon::io::read_at(tx, off, buf_storage.data(), buf_storage.size());
  boost::asio::const_buffer buf = boost::asio::buffer(buf_storage);

  header h;
  h.decode(buf);
  buf += header::SIZE;
  if (h.size > cfg->items_per_node_page) throw tree_error("too many items in tree branch");

  off_ = off;
  parent_off_ = h.parent_off;

  if (h.size == 0) return;
  const auto t = tree();

  // Decode elems_[0].
  std::shared_ptr<abstract_tree_page_branch_elem> e = t->allocate_branch_elem_(allocator);
  assert(buf.size() >= bytes_per_elem);
  e->decode(boost::asio::buffer(buf.data(), bytes_per_elem));
  buf += bytes_per_elem;
  elems_.emplace_back(std::move(e));

  std::shared_ptr<abstract_tree_page_branch_key> k;
  for (std::uint32_t i = 1; i < h.size; ++i) {
    // Decode key separating elems_[i-1] and elems_[i].
    k = t->allocate_branch_key_(allocator);
    assert(buf.size() >= bytes_per_key);
    k->decode(boost::asio::buffer(buf.data(), bytes_per_key));
    buf += bytes_per_key;
    keys_.emplace_back(std::move(k));

    // Decode elems_[i].
    e = t->allocate_branch_elem_(allocator);
    assert(buf.size() >= bytes_per_elem);
    e->decode(boost::asio::buffer(buf.data(), bytes_per_elem));
    buf += bytes_per_elem;
    elems_.emplace_back(std::move(e));
  }

  // Check that the expected not-used bytes is correct.
  assert(buf.size() == (cfg->items_per_node_page - h.size) * (bytes_per_key + bytes_per_elem));
}

void tree_page_branch::encode(txfile::transaction& tx) const {
  if (elems_.size() > cfg->items_per_node_page) throw tree_error("too many items in tree branch");

  const std::size_t bytes_per_elem = abstract_tree_page_branch_elem::offset_size + cfg->augment_bytes;
  const std::size_t bytes_per_key = cfg->key_bytes;

  auto buf_storage = std::vector<std::uint8_t>(encoded_size(*cfg));
  boost::asio::mutable_buffer buf = boost::asio::buffer(buf_storage);

  header h;
  h.magic = magic;
  h.size = elems_.size();
  h.parent_off = parent_off_;
  h.encode(buf);
  buf += header::SIZE;

  // Encode elems_[0].
  assert(buf.size() >= bytes_per_elem);
  elems_[0]->encode(boost::asio::buffer(buf.data(), bytes_per_elem));
  buf += bytes_per_elem;

  for (std::uint32_t i = 1; i < elems_.size(); ++i) {
    // Encode key separating elems_[i-1] and elems_[i].
    assert(buf.size() >= bytes_per_key);
    keys_[i - 1]->encode(boost::asio::buffer(buf.data(), bytes_per_key));
    buf += bytes_per_key;

    // Encode elems_[i].
    assert(buf.size() >= bytes_per_elem);
    elems_[1]->encode(boost::asio::buffer(buf.data(), bytes_per_elem));
    buf += bytes_per_elem;
  }

  assert(buf.size() == (cfg->items_per_node_page - h.size) * (bytes_per_key + bytes_per_elem));
  std::memset(buf.data(), 0, buf.size()); // Zero out unused data.
  monsoon::io::write_at(tx, off_, buf_storage.data(), buf_storage.size());
}

void tree_page_branch::insert_sibling(
    const std::unique_lock<std::shared_mutex>& lck, txfile::transaction& tx,
    const abstract_tree_page& precede_page, std::shared_ptr<abstract_tree_page_branch_elem> precede_augment,
    [[maybe_unused]] const abstract_tree_page& new_sibling, std::shared_ptr<abstract_tree_page_branch_key> sibling_key, std::shared_ptr<abstract_tree_page_branch_elem> sibling_augment,
    abstract_tree::allocator_type tx_allocator,
    tx_op_collection& ops) {
  const std::size_t bytes_per_elem = abstract_tree_page_branch_elem::offset_size + cfg->augment_bytes;
  const std::size_t bytes_per_key = cfg->key_bytes;

  const elems_vector::iterator elems_pos = std::find_if(
      elems_.begin(), elems_.end(),
      [&precede_page](const auto& ptr) -> bool { return ptr->off == precede_page.offset(); });
  if (elems_pos == elems_.end())
    throw std::logic_error("can't insert sibling: preceding page not found");
  const elems_vector::size_type idx = elems_pos - elems_.begin();
  const keys_vector::iterator keys_insert_pos = keys_.begin() + idx;

  // Verify we are not going to exceed size constraints.
  if (elems_.size() >= cfg->items_per_node_page)
    throw std::logic_error("can't insert sibling into full parent page");

  // Validate offsets in elem.
  assert(precede_page.offset() == precede_augment->off);
  assert(new_sibling.offset() == sibling_augment->off);

  // Update size.
  {
    header h;
    h.size = elems_.size() + 1u;
    h.native_to_big_endian();
    monsoon::io::write_at(tx, offset() + offsetof(header, size), &h.size, sizeof(h.size));
  }

  // Update on-disk state.
  const auto write_offset = offset() + header::SIZE + idx * (bytes_per_elem + bytes_per_key);
  const auto write_len = (elems_.size() - idx + 1u) * (bytes_per_elem + bytes_per_key);
  auto buf_storage = std::vector<std::uint8_t>(write_len);
  auto buf = boost::asio::buffer(buf_storage);

  // At elems_[idx], write the updated augmentation.
  assert(buf.size() >= bytes_per_elem);
  precede_augment->encode(boost::asio::buffer(buf.data(), bytes_per_elem));
  buf += bytes_per_elem;
  // At key separating elems_[idx] and elems_[idx+1] write the separator key.
  assert(buf.size() >= bytes_per_key);
  sibling_key->encode(boost::asio::buffer(buf.data(), bytes_per_key));
  buf += bytes_per_key;
  // At elems_[idx+1], write the new sibling reference.
  assert(buf.size() >= bytes_per_elem);
  sibling_augment->encode(boost::asio::buffer(buf.data(), bytes_per_elem));
  buf += bytes_per_elem;

  // Write each (unmodified) element into its new position.
  auto key_write_iter = keys_.begin() + idx;
  auto elem_write_iter = elems_.begin() + idx + 1u;
  while (key_write_iter != keys_.end()) {
    assert(elem_write_iter != elems_.end());

    // Write key separating elements.
    assert(buf.size() >= bytes_per_key);
    (*key_write_iter)->encode(boost::asio::buffer(buf.data(), bytes_per_key));
    buf += bytes_per_key;
    ++key_write_iter;
    // Write element.
    assert(buf.size() >= bytes_per_elem);
    (*elem_write_iter)->encode(boost::asio::buffer(buf.data(), bytes_per_elem));
    buf += bytes_per_elem;
    ++elem_write_iter;
  }
  assert(elem_write_iter == elems_.end());
  assert(buf.size() == 0); // Completely filled the buffer with data.
  monsoon::io::write_at(tx, write_offset, buf_storage.data(), buf_storage.size());

  ops.on_commit(
      [ self=this->shared_from_this(this),
        elems_pos,
        keys_insert_pos,
        elem0=std::move(precede_augment),
        elem1=std::move(sibling_augment),
        key=std::move(sibling_key)
      ]() mutable {
        assert(self != nullptr);
        assert(elem0 != nullptr);
        assert(elem1 != nullptr);
        assert(key != nullptr);
        assert(self->elems_.capacity() > self->elems_.size());
        assert(self->keys_.capacity() > self->keys_.size());
        assert(elems_pos - self->elems_.begin() == keys_insert_pos - self->keys_.begin());

        *elems_pos = std::move(elem0);
        self->elems_.insert(std::next(elems_pos), std::move(elem1));
        self->keys_.insert(keys_insert_pos, std::move(key));

        self.reset();
      });
}

auto tree_page_branch::compute_augment(const std::shared_lock<std::shared_mutex>& lck, abstract_tree::allocator_type allocator) const -> std::shared_ptr<abstract_tree_page_branch_elem> {
  assert(lck.owns_lock() && lck.mutex() == &mtx_());
  return tree()->compute_augment_(off_, { elems_.begin(), elems_.end() }, std::move(allocator));
}

auto tree_page_branch::compute_augment(const std::unique_lock<std::shared_mutex>& lck, abstract_tree::allocator_type allocator) const -> std::shared_ptr<abstract_tree_page_branch_elem> {
  assert(lck.owns_lock() && lck.mutex() == &mtx_());
  return tree()->compute_augment_(off_, { elems_.begin(), elems_.end() }, std::move(allocator));
}

auto tree_page_branch::local_split_(
    const std::unique_lock<std::shared_mutex>& lck, txfile::transaction& tx, std::uint64_t new_page_off,
    cycle_ptr::cycle_gptr<tree_page_branch> parent, const std::unique_lock<std::shared_mutex>& parent_lck,
    abstract_tree::allocator_type sibling_allocator,
    abstract_tree::allocator_type tx_allocator,
    tx_op_collection& ops)
-> std::tuple<
    std::shared_ptr<abstract_tree_page_branch_key>,
    cycle_ptr::cycle_gptr<tree_page_branch>,
    std::unique_lock<std::shared_mutex>
> {
  assert(parent != nullptr);
  assert(parent->offset() == parent_off_);
  assert(lck.owns_lock() && is_my_mutex(lck.mutex()));
  assert(parent_lck.owns_lock() && parent->is_my_mutex(parent_lck.mutex()));

  const auto t = tree();

  // Allocate all objects and figure out parameters.
  if (elems_.size() <= 2u) throw std::logic_error("not enough entries to split branch page");
  const elems_vector::iterator sibling_begin = elems_.begin() + elems_.size() / 2u;
  cycle_ptr::cycle_gptr<tree_page_branch> sibling = t->allocate_branch_(sibling_allocator);
  std::unique_lock<std::shared_mutex> sibling_lck{ sibling->mtx_() };
  const keys_vector::iterator split_key = keys_.begin() + (sibling_begin - elems_.begin() - 1u);
  assert(split_key - keys_.begin() + 1u == sibling_begin - elems_.begin());

  // Initialize sibling.
  sibling->parent_off_ = parent_off_;
  std::copy(
      sibling_begin, elems_.end(),
      std::back_inserter(sibling->elems_));
  std::copy(
      std::next(split_key), keys_.end(),
      std::back_inserter(sibling->keys_));
  assert(sibling->keys_.size() + 1u == sibling->elems_.size());
  sibling->encode(tx);

  // Update size.
  {
    header h;
    h.size = sibling_begin - elems_.begin();
    h.native_to_big_endian();
    monsoon::io::write_at(tx, offset() + offsetof(header, size), &h.size, sizeof(h.size));
  }

  // Compute augments for this and sibling page.
  std::shared_ptr<abstract_tree_page_branch_elem> this_augment = t->compute_augment_(off_, { elems_.begin(), sibling_begin }, parent->allocator);
  std::shared_ptr<abstract_tree_page_branch_elem> sibling_augment = t->compute_augment_(new_page_off, { sibling_begin, elems_.end() }, parent->allocator);

  // Update parent pointer on child pages.
  {
    header h;
    h.parent_off = offset();
    h.native_to_big_endian();
    std::vector<txfile::transaction::offset_type> offsets_of_children;
    std::transform(
        sibling_begin, elems_.end(),
        std::back_inserter(offsets_of_children),
        [](elems_vector::reference elem_ptr) {
          return elem_ptr->off + offsetof(header, parent_off);
        });
    tx.write_at_many(std::move(offsets_of_children), &h.parent_off, sizeof(h.parent_off));
  }

  // On commit:
  // All loaded child pages get their parent offset updated.
  // Release elements we gave to sibling.
  ops.on_commit(
      [self=shared_from_this(this), sibling_begin, new_page_off, t]() {
        // All loaded child pages get their parent offset updated.
        std::for_each(
            sibling_begin, self->elems_.end(),
            [new_page_off, &t, &self](elems_vector::const_reference elem_ptr) {
              const cycle_ptr::cycle_gptr<abstract_tree_page> page = t->get_if_present(elem_ptr->off);
              if (page != nullptr) page->reparent_(self->off_, new_page_off);
            });

        // Release elements we gave to sibling.
        std::fill(sibling_begin, self->elems_.end(), nullptr);
      });

  // Update parent page to have the sibling present.
  parent->insert_sibling(parent_lck, tx, *this, std::move(this_augment), *sibling, *split_key, std::move(sibling_augment), tx_allocator, ops);

  tx.commit();
  return std::forward_as_tuple(
      std::exchange(*split_key, nullptr),
      std::move(sibling),
      std::move(sibling_lck));
}

auto tree_page_branch::local_split_atp_(
    const std::unique_lock<std::shared_mutex>& lck, txfile::transaction& tx, std::uint64_t new_page_off,
    cycle_ptr::cycle_gptr<tree_page_branch> parent, const std::unique_lock<std::shared_mutex>& parent_lck,
    abstract_tree::allocator_type sibling_allocator,
    abstract_tree::allocator_type tx_allocator,
    tx_op_collection& ops)
-> std::tuple<
    std::shared_ptr<abstract_tree_page_branch_key>,
    cycle_ptr::cycle_gptr<abstract_tree_page>,
    std::unique_lock<std::shared_mutex>
> {
  return local_split_(lck, tx, new_page_off, std::move(parent), parent_lck, std::move(sibling_allocator), std::move(tx_allocator), ops);
}

auto tree_page_branch::mtx_() const noexcept -> std::shared_mutex& {
  return mtx_impl_;
}


abstract_tree_elem::~abstract_tree_elem() noexcept = default;

auto abstract_tree_elem::lock_parent_for_read() const
-> std::tuple<cycle_ptr::cycle_gptr<tree_page_leaf>, std::shared_lock<std::shared_mutex>> {
  std::shared_lock<std::shared_mutex> self_lck{ mtx_ref_() };
  return lock_parent_for_read(self_lck);
}

auto abstract_tree_elem::lock_parent_for_read(std::shared_lock<std::shared_mutex>& self_lck) const
-> std::tuple<cycle_ptr::cycle_gptr<tree_page_leaf>, std::shared_lock<std::shared_mutex>> {
  assert(self_lck.owns_lock() && self_lck.mutex() == &mtx_ref_());

  for (;;) {
    assert(self_lck.owns_lock());

    cycle_ptr::cycle_gptr<tree_page_leaf> p = parent_;
    std::shared_lock<std::shared_mutex> p_lck(p->mtx_(), std::try_to_lock);
    if (p_lck.owns_lock()) return std::make_tuple(std::move(p), std::move(p_lck));

    self_lck.unlock();
    p_lck.lock();
    self_lck.lock();
    if (p == parent_) return std::make_tuple(std::move(p), std::move(p_lck));
  }
}

auto abstract_tree_elem::lock_parent_for_write() const
-> std::tuple<cycle_ptr::cycle_gptr<tree_page_leaf>, std::unique_lock<std::shared_mutex>> {
  std::shared_lock<std::shared_mutex> self_lck{ mtx_ref_() };
  return lock_parent_for_write(self_lck);
}

auto abstract_tree_elem::lock_parent_for_write(std::shared_lock<std::shared_mutex>& self_lck) const
-> std::tuple<cycle_ptr::cycle_gptr<tree_page_leaf>, std::unique_lock<std::shared_mutex>> {
  assert(self_lck.owns_lock() && self_lck.mutex() == &mtx_ref_());

  for (;;) {
    assert(self_lck.owns_lock());

    cycle_ptr::cycle_gptr<tree_page_leaf> p = parent_;
    std::unique_lock<std::shared_mutex> p_lck(p->mtx_(), std::try_to_lock);
    if (p_lck.owns_lock()) return std::make_tuple(std::move(p), std::move(p_lck));

    self_lck.unlock();
    p_lck.lock();
    self_lck.lock();
    if (p == parent_) return std::make_tuple(std::move(p), std::move(p_lck));
  }
}

auto abstract_tree_elem::is_never_visible() const noexcept -> bool {
  return false;
}

auto abstract_tree_elem::next() const -> cycle_ptr::cycle_gptr<abstract_tree_elem> {
  std::shared_lock<std::shared_mutex> self_lck{ mtx_ref_() };
  if (succ_ != nullptr) return succ_;

  auto locked_page = lock_parent_for_read(self_lck);
  assert(std::get<0>(locked_page) != nullptr);
  if (succ_ != nullptr) return succ_; // Recheck, because parent lock may have released our lock.
  self_lck.unlock(); // No longer needed.

  for (;;) {
    auto succ_page = std::get<0>(locked_page)->next(std::get<1>(locked_page));
    if (succ_page == nullptr) return nullptr;
    std::shared_lock<std::shared_mutex> succ_page_lck{ succ_page->mtx_() };
    locked_page = std::forward_as_tuple(std::move(succ_page), std::move(succ_page_lck));

    const auto& elems = std::get<0>(locked_page)->elems_;
    const auto first_elem = std::find_if(
        elems.begin(), elems.end(),
        [](const auto& ptr) -> bool { return ptr != nullptr; });
    if (first_elem != elems.end()) return *first_elem;
  }
}

auto abstract_tree_elem::prev() const -> cycle_ptr::cycle_gptr<abstract_tree_elem> {
  for (;;) {
    std::shared_lock<std::shared_mutex> self_lck{ mtx_ref_() };
    if (pred_ != nullptr) return pred_;

    auto locked_page = lock_parent_for_read(self_lck);
    assert(std::get<0>(locked_page) != nullptr);
    if (pred_ != nullptr) return pred_;
    self_lck.unlock(); // No longer needed and would be an issue with lock ordering.

    bool validation_needed = false;
    cycle_ptr::cycle_gptr<abstract_tree_elem> result = nullptr;
    for (;;) {
      auto pred_page = std::get<0>(locked_page)->prev(std::get<1>(locked_page));
      if (pred_page == nullptr) break;
      std::shared_lock<std::shared_mutex> pred_page_lck{ pred_page->mtx_(), std::try_to_lock };
      if (!pred_page_lck.owns_lock()) {
        std::get<1>(locked_page).unlock();
        pred_page_lck.lock();
        if (!validation_needed) {
          std::get<1>(locked_page).lock();
          if (std::get<0>(locked_page)->prev(std::get<1>(locked_page)) != pred_page) validation_needed = true;
        }
      }
      locked_page = std::forward_as_tuple(std::move(pred_page), std::move(pred_page_lck));

      const auto& elems = std::get<0>(locked_page)->elems_;
      const auto last_elem = std::find_if(
          elems.rbegin(), elems.rend(),
          [](const auto& ptr) -> bool { return ptr != nullptr; });
      if (last_elem != elems.rend()) {
        result = *last_elem;
        break;
      }
    }
    // Release locks and references.
    std::get<1>(locked_page).unlock();

    if (!validation_needed) return result;
    if (result == nullptr) {
      // Figure out how to test if result is the first element.
      if (std::get<0>(locked_page)->tree()->first_element_().get() == this) return result;
    } else {
      // Use forward iteration to fix up result, if needed.
      for (auto result_succ = result->next();
          result_succ != nullptr;
          result_succ = (result = std::move(result_succ))->next()) {
        // If we are the successor of the found element, we have found our predecessor.
        if (result_succ.get() == this) return result;
      }
    }
  }
}


abstract_tx_aware_tree_elem::~abstract_tx_aware_tree_elem() noexcept = default;

auto abstract_tx_aware_tree_elem::mtx_ref_() const noexcept -> std::shared_mutex& {
  return mtx_;
}

auto abstract_tx_aware_tree_elem::offset() const -> std::uint64_t {
  // Parent should be locked, so this is safe.
  return parent_->offset_for_(*this);
}

auto abstract_tx_aware_tree_elem::get_container_for_layout() const -> cycle_ptr::cycle_gptr<const detail::layout_obj> {
  return parent_;
}


abstract_tree_page_branch_elem::~abstract_tree_page_branch_elem() noexcept = default;


abstract_tree_page_branch_key::~abstract_tree_page_branch_key() noexcept = default;


abstract_tree::for_each_augment_layer_::for_each_augment_layer_(cycle_ptr::cycle_gptr<const tree_page_branch> page, cheap_fn_ref<bool(const abstract_tree_page_branch_elem&)> filter)
: page_(std::move(page)),
  lck_(page_->mtx_()),
  iter_(std::find_if(
          page_->elems_.begin(), page_->elems_.end(),
          [&filter](const auto& elem_ptr) -> bool {
            return elem_ptr != nullptr && filter(*elem_ptr);
          }))
{}

auto abstract_tree::for_each_augment_layer_::next_page(cheap_fn_ref<bool(const abstract_tree_page_branch_elem&)> filter) -> cycle_ptr::cycle_gptr<abstract_tree_page> {
  if (iter_ == page_->elems_.end()) return nullptr;
  cycle_ptr::cycle_gptr<abstract_tree_page> result = page_->tree()->get((*iter_)->off);
  // Move iterator to the next element.
  iter_ = std::find_if(
      std::next(iter_), page_->elems_.end(),
      [&filter](const auto& elem_ptr) -> bool {
        return elem_ptr != nullptr && filter(*elem_ptr);
      });
  return result;
}


template class tree_page_branch_elem<>;


} /* namespace monsoon::tx::detail */
