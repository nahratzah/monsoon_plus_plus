#include <monsoon/tx/detail/commit_manager.h>
#include <monsoon/tx/db_errc.h>
#include <monsoon/io/rw.h>
#include <cassert>
#include <stdexcept>
#include <type_traits>

namespace monsoon::tx::detail {
namespace {


struct commit_manager_buffer {
  std::uint32_t magic;
  commit_manager::type tx_start;
  commit_manager::type last_write;
  commit_manager::type completed_commit;
};

static_assert(sizeof(commit_manager_buffer) == commit_manager::SIZE,
    "structure for encoding/decoding has the wrong size");


template<typename Fn>
auto cm_do_noexcept_(Fn&& fn) noexcept -> decltype(auto) {
  return std::invoke(std::forward<Fn>(fn));
}


} /* namespace monsoon::tx::detail */


commit_manager::commit_manager(const txfile& f, monsoon::io::fd::offset_type off, allocator_type alloc)
: off_(off),
  alloc_(alloc)
{
  auto tx = f.begin();
  commit_manager_buffer buf;
  monsoon::io::read_at(tx, off, &buf, sizeof(buf));

  boost::endian::big_to_native_inplace(buf.magic);
  boost::endian::big_to_native_inplace(buf.tx_start);
  boost::endian::big_to_native_inplace(buf.last_write);
  boost::endian::big_to_native_inplace(buf.completed_commit);

  if (buf.magic != magic)
    throw std::runtime_error("commit_manager: magic mismatch");
  tx_start_ = buf.tx_start;
  last_write_commit_id_ = buf.last_write;
  completed_commit_id_ = buf.completed_commit;
}

commit_manager::~commit_manager() noexcept = default;

auto commit_manager::allocate(const txfile& f, monsoon::io::fd::offset_type off, allocator_type alloc) -> std::shared_ptr<commit_manager> {
  auto cm_ptr = std::allocate_shared<commit_manager>(alloc, f, off, alloc);
  assert(alloc == cm_ptr->alloc_);

  auto s = std::allocate_shared<state_>(alloc, cm_ptr->tx_start_, *cm_ptr);
  cm_ptr->states_.push_back(*s); // Never throws.
  cm_ptr->s_ = std::move(s);

  return cm_ptr;
}

void commit_manager::init(txfile::transaction& tx, monsoon::io::fd::offset_type off) {
  commit_manager_buffer buf;
  buf.magic = magic;
  buf.tx_start = 0;
  buf.last_write = 0;
  buf.completed_commit = 0;

  boost::endian::native_to_big_inplace(buf.magic);
  boost::endian::native_to_big_inplace(buf.tx_start);
  boost::endian::native_to_big_inplace(buf.last_write);
  boost::endian::native_to_big_inplace(buf.completed_commit);

  monsoon::io::write_at(tx, off, &buf, sizeof(buf));
}

auto commit_manager::get_tx_commit_id() const -> commit_id {
  std::shared_lock<std::shared_mutex> lck{ mtx_ };
  assert(s_ != nullptr);
  return commit_id(completed_commit_id_, s_);
}

auto commit_manager::prepare_commit(txfile& f) -> write_id {
  auto tx = f.begin(false); // WAL-transaction for the commit-id transaction.
  std::lock_guard<std::shared_mutex> lck{ mtx_ };

  // Don't start a transaction if we've exhausted our delta.
  if (last_write_commit_id_ - tx_start_ >= max_tx_delta)
    throw std::runtime_error("too many transactions without vacuuming");

  // Write the update to last_write_commit_id to disk now,
  // so the ID can never again be selected.
  if (last_write_commit_id_avail_ == 0) {
    // Figure out new end-of-allocation value.
    // This uses unsigned-overflow arithmatic.
    type new_val = tx_start_ + max_tx_delta;
    if (new_val - last_write_commit_id_ > prealloc_batch)
      new_val = last_write_commit_id_ + prealloc_batch;
    assert(new_val != last_write_commit_id_); // Should have been caught by max_tx_delta test above.
    const type new_avail = new_val - last_write_commit_id_;

    // Commit reservation to disk.
    boost::endian::native_to_big_inplace(new_val);
    auto immed_tx = f.begin(false); // Local transaction for updating commit_manager state.
    monsoon::io::write_at(immed_tx, off_ + OFF_LAST_WRITE_COMMIT_ID, &new_val, sizeof(new_val));
    immed_tx.commit(); // May fail, in which case no changes are made to this class.

    // Now that the reservation is written to disk, update our datastructure.
    last_write_commit_id_avail_ = new_avail;
  }

  // Compute transaction ID.
  --last_write_commit_id_avail_;
  assert(s_ != nullptr);
  auto cid = commit_id(++last_write_commit_id_, s_);

  // Write the completed-commit-id update in this transaction.
  const type big_endian_commit_id = boost::endian::native_to_big(cid.val_);
  monsoon::io::write_at(tx, off_ + OFF_COMPLETED_COMMIT_ID, &big_endian_commit_id, sizeof(big_endian_commit_id));

  auto s = allocate_unique<write_id_state_>(traits_type::rebind_alloc<write_id_state_>(alloc_), std::move(cid), std::move(tx));
  writes_.push_back(*s); // Never throws.
  return write_id(std::move(s)); // Never throws.
}

void commit_manager::null_commit_(unique_alloc_ptr<write_id_state_, traits_type::rebind_alloc<write_id_state_>> s) noexcept {
  if (s == nullptr) return;
  assert(s->get_cm_or_null().get() == this);
  assert(std::holds_alternative<std::monostate>(s->wait_));
  // Successful transactions will unlink this.
  if (!s->is_linked()) return;

  std::unique_lock<std::shared_mutex> lck{ mtx_ };

  const auto s_iter = writes_.iterator_to(*s);
  const bool first = (s_iter == writes_.begin());
  writes_.erase(s_iter);

  if (first) maybe_start_front_write_locked_(lck);
}

void commit_manager::maybe_start_front_write_locked_(const std::unique_lock<std::shared_mutex>& lck) noexcept {
  assert(lck.owns_lock() && lck.mutex() == &mtx_);

  bool success;
  do {
    if (writes_.empty()) return;
    success = true;

    auto& wfront = writes_.front();
    std::visit(
        [&success, &lck](auto& x) {
          using x_type = std::decay_t<decltype(x)>;

          if constexpr(std::is_same_v<std::condition_variable_any, x_type>) {
            // Pessimistic wake-up:
            // the transaction otherwise may wake up spuriously between the unlock
            // and the notify, complete, and go away.
            // And if we were that unlucky, we notify_one would target a dangling
            // reference.
            x.notify_one();
          } else {
            static_assert(std::is_same_v<std::monostate, x_type>);
            // Nothing to do if the thread isn't blocked.
          }
        },
        wfront.wait_);
    // If we fail, we maintain the lock.
    assert(success || lck.owns_lock());
    // If we fail, the front will be different.
    assert(success || &wfront != &writes_.front());
  } while (!success);
}


void commit_manager::write_id_state_::on_pimpl_release(unique_alloc_ptr<write_id_state_, traits_type::rebind_alloc<write_id_state_>>&& pimpl) noexcept {
  if (pimpl != nullptr) {
    auto cm = pimpl->get_cm_or_null();
    if (cm != nullptr) cm->null_commit_(std::move(pimpl));
  }
}

void commit_manager::write_id_state_::wait_until_front_transaction_(const commit_manager& cm) {
  std::shared_lock<std::shared_mutex> lck{ cm.mtx_ };
  assert(std::holds_alternative<std::monostate>(wait_));
  cm_do_noexcept_(
      [&]() {
        auto& cv = wait_.emplace<std::condition_variable_any>();
        cv.wait(lck,
            [&cm, this]() -> bool {
              assert(!cm.writes_.empty()); // Can't be empty, because we're queued.
              return this == &cm.writes_.front();
            });
        wait_.emplace<std::monostate>(); // Release condition variable since we no longer need it.
      });
}


} /* namespace monsoon::tx::detail */
