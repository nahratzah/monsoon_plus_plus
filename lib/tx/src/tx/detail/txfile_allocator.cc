#include <monsoon/tx/detail/txfile_allocator.h>
#include <monsoon/io/rw.h>
#include <algorithm>
#include <array>
#include <iterator>
#include <mutex>
#include <tuple>
#include <vector>
#include <boost/endian/conversion.hpp>
#include <boost/polymorphic_pointer_cast.hpp>
#include <boost/polymorphic_cast.hpp>
#include <boost/asio/buffer.hpp>
#include <objpipe/of.h>
#include <cycle_ptr/cycle_ptr.h>

namespace monsoon::tx::detail {


void txfile_allocator::key::decode(boost::asio::const_buffer buf) {
  boost::asio::buffer_copy(boost::asio::buffer(&addr, sizeof(addr)), buf);
  boost::endian::big_to_native_inplace(addr);
}

void txfile_allocator::key::encode(boost::asio::mutable_buffer buf) const {
  const auto be_addr = boost::endian::native_to_big(addr);
  boost::asio::buffer_copy(buf, boost::asio::buffer(&be_addr, sizeof(be_addr)));
}


txfile_allocator::element::~element() noexcept = default;

void txfile_allocator::element::decode(boost::asio::const_buffer buf) {
  boost::asio::buffer_copy(
      std::array<boost::asio::mutable_buffer, 3> {{
        boost::asio::buffer(&key.addr, sizeof(key.addr)),
        boost::asio::buffer(&used, sizeof(used)),
        boost::asio::buffer(&free, sizeof(free)),
      }},
      buf);

  boost::endian::big_to_native_inplace(key.addr);
  boost::endian::big_to_native_inplace(used);
  boost::endian::big_to_native_inplace(free);
}

void txfile_allocator::element::encode(boost::asio::mutable_buffer buf) const {
  const auto be_addr = boost::endian::native_to_big(key.addr);
  const auto be_used = boost::endian::native_to_big(used);
  const auto be_free = boost::endian::native_to_big(free);

  boost::asio::buffer_copy(
      buf,
      std::array<boost::asio::const_buffer, 3>{{
        boost::asio::buffer(&be_addr, sizeof(be_addr)),
        boost::asio::buffer(&be_used, sizeof(be_used)),
        boost::asio::buffer(&be_free, sizeof(be_free))
      }});
}

auto txfile_allocator::element::is_never_visible() const noexcept -> bool {
  return used == 0 && free == 0;
}

auto txfile_allocator::element::mtx_ref_() const noexcept -> std::shared_mutex& {
  return mtx_;
}

auto txfile_allocator::element::branch_key_(allocator_type alloc) const -> std::shared_ptr<abstract_tree_page_branch_key> {
  return std::allocate_shared<tree_page_branch_key<class key>>(alloc, key);
}


void txfile_allocator::max_free_space_augment::decode(boost::asio::const_buffer buf) {
  boost::asio::buffer_copy(boost::asio::buffer(&free, sizeof(free)), buf);
  boost::endian::big_to_native_inplace(free);
}

void txfile_allocator::max_free_space_augment::encode(boost::asio::mutable_buffer buf) const {
  const auto be_free = boost::endian::native_to_big(free);
  boost::asio::buffer_copy(buf, boost::asio::buffer(&be_free, sizeof(be_free)));
}


txfile_allocator::txfile_allocator(std::shared_ptr<monsoon::tx::db> db, std::uint64_t off)
: abstract_tree(),
  db_obj(db)
{}

txfile_allocator::~txfile_allocator() noexcept = default;

auto txfile_allocator::compute_augment_(std::uint64_t off, const std::vector<cycle_ptr::cycle_gptr<const abstract_tree_elem>>& elems, allocator_type allocator) const -> std::shared_ptr<abstract_tree_page_branch_elem> {
  auto augments = objpipe::of(std::cref(elems))
      .iterate()
      .filter([](const auto& ptr) noexcept -> bool { return ptr != nullptr; })
      .transform(
          [](const auto& ptr) noexcept -> cycle_ptr::cycle_gptr<const element> {
            return boost::polymorphic_pointer_downcast<const element>(ptr);
          })
      .deref()
      .transform(
          [](const auto& ref) noexcept {
            return std::make_tuple(
                max_free_space_augment(ref));
          })
      .reduce(&txfile_allocator::augment_merge_)
      .value_or(std::tuple<max_free_space_augment>());

  return std::allocate_shared<branch_elem>(allocator, off, std::move(augments));
}

auto txfile_allocator::compute_augment_(std::uint64_t off, const std::vector<std::shared_ptr<const abstract_tree_page_branch_elem>>& elems, allocator_type allocator) const -> std::shared_ptr<abstract_tree_page_branch_elem> {
  auto augments = objpipe::of(std::cref(elems))
      .iterate()
      .filter([](const auto& ptr) noexcept -> bool { return ptr != nullptr; })
      .transform(
          [](const auto& ptr) noexcept -> std::shared_ptr<const branch_elem> {
            return boost::polymorphic_pointer_downcast<const branch_elem>(ptr);
          })
      .transform(
          [](const auto& ptr) noexcept {
            return ptr->augments;
          })
      .reduce(&txfile_allocator::augment_merge_)
      .value_or(std::tuple<max_free_space_augment>());

  return std::allocate_shared<branch_elem>(allocator, off, std::move(augments));
}

auto txfile_allocator::allocate_elem_(cycle_ptr::cycle_gptr<tree_page_leaf> parent, allocator_type allocator) const -> cycle_ptr::cycle_gptr<abstract_tree_elem> {
  return cycle_ptr::allocate_cycle<element>(allocator, parent);
}

auto txfile_allocator::allocate_branch_elem_(allocator_type allocator) const -> std::shared_ptr<abstract_tree_page_branch_elem> {
  return std::allocate_shared<branch_elem>(allocator);
}

auto txfile_allocator::allocate_branch_key_(allocator_type allocator) const -> std::shared_ptr<abstract_tree_page_branch_key> {
  return std::allocate_shared<tree_page_branch_key<class key>>(allocator);
}

auto txfile_allocator::less_cb(const abstract_tree_page_branch_key&  x, const abstract_tree_page_branch_key&  y) const -> bool {
  auto* cx = boost::polymorphic_cast<const tree_page_branch_key<class key>*>(&x);
  auto* cy = boost::polymorphic_cast<const tree_page_branch_key<class key>*>(&y);
  return cx->key < cy->key;
}

auto txfile_allocator::less_cb(const abstract_tree_elem&             x, const abstract_tree_elem&             y) const -> bool {
  auto* cx = boost::polymorphic_cast<const element*>(&x);
  auto* cy = boost::polymorphic_cast<const element*>(&y);
  return cx->key < cy->key;
}

auto txfile_allocator::less_cb(const abstract_tree_page_branch_key&  x, const abstract_tree_elem&             y) const -> bool {
  auto* cx = boost::polymorphic_cast<const tree_page_branch_key<class key>*>(&y);
  auto* cy = boost::polymorphic_cast<const element*>(&x);
  return cx->key < cy->key;
}

auto txfile_allocator::less_cb(const abstract_tree_elem&             x, const abstract_tree_page_branch_key&  y) const -> bool {
  auto* cx = boost::polymorphic_cast<const element*>(&x);
  auto* cy = boost::polymorphic_cast<const tree_page_branch_key<class key>*>(&y);
  return cx->key < cy->key;
}

auto txfile_allocator::augment_merge_(const std::tuple<max_free_space_augment>& x, const std::tuple<max_free_space_augment>& y) -> std::tuple<max_free_space_augment> {
  return std::make_tuple(
      max_free_space_augment::merge(std::get<0>(x), std::get<0>(y)));
}

auto txfile_allocator::allocate_txfile_bytes(txfile::transaction& tx, std::uint64_t bytes, allocator_type tx_allocator, tx_op_collection& ops) -> std::uint64_t {
  using action = txfile_allocator_log::action;

  {
    const auto opt_result = maybe_allocate_txfile_bytes_from_tree_(tx, bytes, tx_allocator, ops);
    if (opt_result.has_value()) return *opt_result;
  }

  // If we can't allocate from the tree, we'll have to grow the file.
  auto spc_log_entry = log_->allocate_by_growing_file(
      *f_, bytes,
      [this, tx_allocator](txfile::transaction& tx, std::uint64_t bytes, tx_op_collection& ops) -> std::optional<std::uint64_t> {
        return steal_allocate_(tx, bytes, tx_allocator, ops);
      },
      tx_allocator);

  spc_log_entry->on_commit(tx, action::used, ops);
  return spc_log_entry->get_addr();
}

auto txfile_allocator::maybe_allocate_txfile_bytes_from_tree_(txfile::transaction& tx, std::uint64_t bytes, allocator_type tx_allocator, tx_op_collection& ops) -> std::optional<std::uint64_t> {
  using action = txfile_allocator_log::action;

  // Allocate a log record to hold the free space.
  // We must do this before allocating space, because locking the same page twice
  // may cause dead-lock if another thread is trying to acquire a write-lock.
  const auto new_log_record = log_->new_entry(*f_,
      action::skip, 0, 0,
      [this, tx_allocator](txfile::transaction& tx, std::uint64_t bytes, tx_op_collection& ops) {
        return steal_allocate_(tx, bytes, tx_allocator, ops);
      },
      tx_allocator);

  // Init the commit logic.
  // We use a separate transaction to move the bytes to a log record.
  // And then the actual transaction is used to modify the log record.
  {
    auto split_tx = f_->begin(false);
    tx_op_collection split_ops(tx_allocator);

    // Allocate bytes using the "stealing" allocator.
    const auto opt_addr = steal_allocate_(split_tx, bytes, tx_allocator, split_ops);
    if (!opt_addr.has_value()) return {};

    // "Unsteal" the space by recording it in a log entry.
    new_log_record->modify_addr_len(split_tx, *opt_addr, bytes, split_ops);
    new_log_record->on_commit(split_tx, action::free, split_ops);

    split_tx.commit();
    split_ops.commit(); // Never throws.
  }

  // Now attach the log record to the actual allocation request.
  new_log_record->on_commit(tx, action::used, ops);
  // Return the new address.
  return new_log_record->get_addr();
}

auto txfile_allocator::steal_allocate_(
    txfile::transaction& tx, std::uint64_t bytes,
    allocator_type tx_allocator,
    tx_op_collection& ops)
-> std::optional<std::uint64_t> {
  using action = txfile_allocator_log::action;

  struct context {
    cycle_ptr::cycle_gptr<element> use_elem;
    std::unique_lock<std::shared_mutex> use_elem_lck;
    // We need to keep the leaf locked for reading,
    // because we need to prevent it from having its layout changed.
    cycle_ptr::cycle_gptr<tree_page_leaf> use_leaf;
    std::shared_lock<std::shared_mutex> use_leaf_lck;
  };

  // Use a shared pointer so we can maintain the locks during the transaction.
  const auto ctx = std::allocate_shared<context>(tx_allocator);

  // Search for an element with enough free space.
  with_for_each_augment_for_read(
      [bytes](const abstract_tree_page_branch_elem& raw_elem) -> bool {
        const branch_elem& elem = *boost::polymorphic_downcast<const branch_elem*>(&raw_elem);
        return std::get<max_free_space_augment>(elem.augments).free >= bytes;
      },
      [ out=std::tie(ctx->use_elem, ctx->use_elem_lck, ctx->use_leaf, ctx->use_leaf_lck),
        bytes
      ](cycle_ptr::cycle_gptr<abstract_tree_elem> raw_elem, cycle_ptr::cycle_gptr<tree_page_leaf> leaf, std::shared_lock<std::shared_mutex>& leaf_lck) mutable {
        cycle_ptr::cycle_gptr<element> elem = boost::polymorphic_pointer_downcast<element>(raw_elem);
        std::unique_lock<std::shared_mutex> elem_lck{ elem->mtx_ref_() };

        // Reject elements with insufficient space.
        if (elem->free < bytes) return stop_continue::sc_continue;

        // Steal all the variables.
        out = std::forward_as_tuple(std::move(elem), std::move(elem_lck), std::move(leaf), std::move(leaf_lck));
        return stop_continue::sc_stop;
      });

  if (ctx->use_elem == nullptr) return {};
  // Compute the offset of the use_elem.
  const auto use_offset = ctx->use_leaf->offset_for_(*ctx->use_elem);

  // Claim bytes from element.
  ctx->use_elem->free -= bytes;
  ops.on_rollback(
      [ctx, bytes]() {
        ctx->use_elem->free += bytes;
      });
  std::array<std::uint8_t, element::SIZE> buf;
  ctx->use_elem->encode(boost::asio::buffer(buf));
  monsoon::io::write_at(tx, use_offset, buf.data(), buf.size());

  // XXX: We don't update the augments.
  // We _ought_ to, but at the moment it's harmless not to.

  // Return the allocated address.
  return ctx->use_elem->key.addr + ctx->use_elem->used + ctx->use_elem->free;
}

void txfile_allocator::do_maintenance_(allocator_type tx_allocator) {
  using action = txfile_allocator_log::action;

  bool made_progress = false;
  try {
    auto maintenance_queue = log_->maintenance(
        *f_,
        [this, tx_allocator](txfile::transaction& tx, std::uint64_t bytes, tx_op_collection& ops) -> std::optional<std::uint64_t> {
          return steal_allocate_(tx, bytes, tx_allocator, ops);
        },
        tx_allocator);

    for (auto& record : maintenance_queue) {
      // First, insert an entry that holds no space.
      bool ins_succeeded;
      cycle_ptr::cycle_gptr<element> ins_pos;
      {
        cycle_ptr::cycle_gptr<abstract_tree_elem> abstract_ins_pos;
        std::tie(abstract_ins_pos, ins_succeeded) = insert_(
            tree_page_branch_key<key>(key(record->get_addr())),
            [k=key(record->get_addr())](allocator_type allocator, cycle_ptr::cycle_gptr<tree_page_leaf> parent) -> cycle_ptr::cycle_gptr<abstract_tree_elem> {
              return cycle_ptr::allocate_cycle<element>(allocator, parent, k);
            },
            tx_allocator);
        ins_pos = boost::polymorphic_pointer_downcast<element>(abstract_ins_pos);
      }

      // Maybe this should be an assertion, as it indicates a bug in the allocator.
      if (!ins_succeeded && (ins_pos->used !=0 || ins_pos->free != 0))
        throw std::logic_error("tree corruption: element marked free twice!");

      // Lock the layout down.
      const auto locked_parent = ins_pos->lock_parent_for_write();
      // Also lock the element for write.
      std::unique_lock<std::shared_mutex> ins_pos_lck{ ins_pos->mtx_ref_() };

      // Successor page (optional).
      cycle_ptr::cycle_gptr<tree_page_leaf> next_page;
      std::unique_lock<std::shared_mutex> next_page_lck;
      // Successor element (optional).
      cycle_ptr::cycle_gptr<element> succ;
      std::unique_lock<std::shared_mutex> succ_lck;

      // Start the transaction logic.
      // Note that by placing the locks before it,
      // we ensure the ops commit/rollback will happen with locks held.
      auto tx = f_->begin(false);
      tx_op_collection ops(tx_allocator);

      // Update the inserted element.
      switch (record->get_action()) {
        default:
          continue; // Shouldn't happen.
        case action::free:
          ins_pos->free = record->get_len();
          break;
        case action::used:
          ins_pos->used = record->get_len();
          break;
      }
      // Undo operation.
      ops.on_rollback(
          [ins_pos]() {
            ins_pos->free = ins_pos->used = 0;
          });

      // See if we can merge in space from successor.
      if (ins_pos->succ_ != nullptr) {
        succ = boost::polymorphic_pointer_downcast<element>(ins_pos->succ_);
        succ_lck = std::unique_lock<std::shared_mutex>(succ->mtx_ref_());
      } else {
        // Find successor in the next page(s).
        assert(succ == nullptr);
        next_page = std::get<0>(locked_parent)->next(std::get<1>(locked_parent));
        if (next_page != nullptr)
          next_page_lck = std::unique_lock<std::shared_mutex>(next_page->mtx_());
        while (next_page != nullptr) {
          const auto succ_iter = std::find_if(next_page->elems_.begin(), next_page->elems_.end(),
              [](const auto& ptr) { return ptr != nullptr; });
          if (succ_iter != next_page->elems_.end()) {
            succ = boost::polymorphic_pointer_downcast<element>(*succ_iter);
            succ_lck = std::unique_lock<std::shared_mutex>(succ->mtx_ref_());
            break;
          }

          // Advance.
          auto new_next_page = next_page->next(std::get<1>(locked_parent));
          std::unique_lock<std::shared_mutex> new_next_page_lck;
          if (new_next_page != nullptr)
            new_next_page_lck = std::unique_lock<std::shared_mutex>(next_page->mtx_());
          next_page_lck.unlock();
          next_page = std::move(new_next_page);
          next_page_lck = std::move(new_next_page_lck);
        }
      }
      // Try to claim all the space from the successor. Requires zero usage.
      if (succ == nullptr) {
        // Skip: can't steal from successor if you don't have a successor.
      } else if (succ->free == 0) {
        // There is nothing to steal here.
        // Also: if used==0 && free==0, then the element is likely being deallocated somewhere.
        succ_lck.unlock();
      } else if (succ->used != 0 || succ->key.addr != ins_pos->key.addr + ins_pos->used + ins_pos->free) {
        // Also can't steal if your successor's free space doesn't connect to yours.
        succ_lck.unlock();
      } else {
        // Claim the space from successor.
        ins_pos->free += succ->free;
        ops.on_commit(
            [succ]() {
              succ->free = 0;
            });

        std::array<std::uint8_t, element::SIZE> buf;
        succ->encode(boost::asio::buffer(buf));
        monsoon::io::write_at(
            tx, std::get<0>(locked_parent)->offset_for_(*succ),
            buf.data(), buf.size());
      }

      std::array<std::uint8_t, element::SIZE> buf;
      ins_pos->encode(boost::asio::buffer(buf));
      monsoon::io::write_at(
          tx, std::get<0>(locked_parent)->offset_for_(*ins_pos),
          buf.data(), buf.size());

      // This operation moves the record into the tree, so the record has to release its claim.
      record->on_commit(tx, action::skip, ops);

      tx.commit();
      ops.commit(); // Never throws.

      // XXX if succ!=nullptr, must delete it from its page

      // XXX above code doesn't merge free space with predecessor. Fix that.
      // Hint: probably easy to, if the predecessor is nulled and added to the log (similar to how allocations are done).

      made_progress = true;
      record.reset(); // Frees up the record.
    }
  } catch (const std::bad_alloc&) {
    if (!made_progress) throw;
  }
}


} /* namespace monsoon::tx::detail */
