#include <monsoon/tx/detail/commit_manager_impl.h>
#include <monsoon/tx/db_errc.h>
#include <monsoon/io/rw.h>
#include <cassert>
#include <stdexcept>
#include <type_traits>
#include <boost/endian/conversion.hpp>

namespace monsoon::tx::detail {
namespace {


struct commit_manager_buffer {
  std::uint32_t magic;
  commit_manager::type tx_start;
  commit_manager::type last_write;
  commit_manager::type completed_commit;
};

static_assert(sizeof(commit_manager_buffer) == commit_manager_impl::SIZE,
    "structure for encoding/decoding has the wrong size");


template<typename Fn>
auto cm_do_noexcept_(Fn&& fn) noexcept -> decltype(auto) {
  return std::invoke(std::forward<Fn>(fn));
}


} /* namespace monsoon::tx::detail */


commit_manager_impl::commit_manager_impl(monsoon::io::fd::offset_type off, allocator_type alloc)
: off_(off),
  alloc_(alloc)
{}

commit_manager_impl::~commit_manager_impl() noexcept = default;

auto commit_manager_impl::allocate(const txfile& f, monsoon::io::fd::offset_type off, allocator_type alloc) -> std::shared_ptr<commit_manager_impl> {
  class impl
  : public commit_manager_impl
  {
    public:
    impl(monsoon::io::fd::offset_type off, allocator_type alloc)
    : commit_manager_impl(off, std::move(alloc))
    {}
  };

  auto tx = f.begin();
  commit_manager_buffer buf;
  monsoon::io::read_at(tx, off, &buf, sizeof(buf));

  boost::endian::big_to_native_inplace(buf.magic);
  boost::endian::big_to_native_inplace(buf.tx_start);
  boost::endian::big_to_native_inplace(buf.last_write);
  boost::endian::big_to_native_inplace(buf.completed_commit);

  if (buf.magic != magic)
    throw std::runtime_error("commit_manager: magic mismatch");

  std::shared_ptr<commit_manager_impl> cm_ptr = std::allocate_shared<impl>(alloc, off, alloc);
  assert(alloc == cm_ptr->alloc_);

  cm_ptr->tx_start_ = buf.tx_start;
  cm_ptr->last_write_commit_id_ = buf.last_write;

  auto s = std::allocate_shared<state_impl_>(alloc, buf.tx_start, buf.completed_commit, *cm_ptr);
  cm_ptr->states_.push_back(*s); // Never throws.
  cm_ptr->completed_commit_id_ = make_commit_id(s);

  return cm_ptr;
}

void commit_manager_impl::init(txfile::transaction& tx, monsoon::io::fd::offset_type off) {
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

auto commit_manager_impl::get_tx_commit_id() const -> commit_id {
  std::shared_lock<std::shared_mutex> lck{ mtx_ };
  assert(s_ != nullptr);
  return completed_commit_id_;
}

auto commit_manager_impl::prepare_commit(txfile& f) -> write_id {
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
  auto cid_state = std::allocate_shared<state_impl_>(alloc_, tx_start_, ++last_write_commit_id_, *this);
  states_.push_back(*cid_state); // Never throws.
  auto cid = make_commit_id(cid_state);

  // Write the completed-commit-id update in this transaction.
  const type big_endian_commit_id = boost::endian::native_to_big(cid.val());
  monsoon::io::write_at(tx, off_ + OFF_COMPLETED_COMMIT_ID, &big_endian_commit_id, sizeof(big_endian_commit_id));

  auto s = std::allocate_shared<write_id_state_impl_>(traits_type::rebind_alloc<write_id_state_>(alloc_), std::move(cid), std::move(tx));
  writes_.push_back(*s); // Never throws.
  return make_write_id(std::move(s)); // Never throws.
}

void commit_manager_impl::null_commit_(write_id_state_impl_& s) noexcept {
  assert(s.get_cm_or_null().get() == this);
  assert(std::holds_alternative<std::monostate>(s.wait_));

  std::unique_lock<std::shared_mutex> lck{ mtx_ };
  // Successful transactions will unlink this.
  assert(!s.is_linked());

  const auto s_iter = writes_.iterator_to(s);
  const bool first = (s_iter == writes_.begin());
  writes_.erase(s_iter);

  if (first) maybe_start_front_write_locked_(lck);
}

void commit_manager_impl::maybe_start_front_write_locked_(const std::unique_lock<std::shared_mutex>& lck) noexcept {
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


commit_manager_impl::state_impl_::~state_impl_() noexcept {
  if (is_linked()) {
    std::shared_ptr<commit_manager_impl> cm = this->cm.lock();
    if (cm != nullptr) {
      std::lock_guard<std::shared_mutex>{ cm->mtx_ };
      cm->states_.erase(cm->states_.iterator_to(*this));
    }
  }
}

auto commit_manager_impl::state_impl_::get_cm_or_null() const noexcept -> std::shared_ptr<commit_manager> {
  return cm.lock();
}


commit_manager_impl::write_id_state_impl_::~write_id_state_impl_() noexcept {
  if (is_linked()) {
    const auto raw_cm = get_cm_or_null();
    if (raw_cm != nullptr) {
#ifdef NDEBUG
      const auto cm = std::static_pointer_cast<commit_manager_impl>(raw_cm);
#else
      const auto cm = std::dynamic_pointer_cast<commit_manager_impl>(raw_cm);
      assert(cm != nullptr);
#endif
      cm->null_commit_(*this);
    }
  }
}

void commit_manager_impl::write_id_state_impl_::wait_until_front_transaction_(const commit_manager_impl& cm) {
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

auto commit_manager_impl::write_id_state_impl_::do_apply(cheap_fn_ref<std::error_code> validation, cheap_fn_ref<> phase2) -> std::error_code {
  const auto raw_cm = get_cm_or_null();
  if (raw_cm == nullptr) return db_errc::gone_away;
#ifdef NDEBUG
  const auto cm = std::static_pointer_cast<commit_manager_impl>(raw_cm);
#else
  const auto cm = std::dynamic_pointer_cast<commit_manager_impl>(raw_cm);
  assert(cm != nullptr);
#endif
  wait_until_front_transaction_(*cm);

  std::unique_lock<std::shared_mutex> commit_lck{ cm->commit_mtx_ };

  std::error_code ec = validation();
  if (ec) return ec; // Validation failure.

  const auto delay_release_of_old_cid = cm->completed_commit_id_;
  std::unique_lock<std::shared_mutex> wlck{ cm->mtx_ };
  tx_.commit(); // May throw.

  // Remaining code runs under no-except clause to maintain invariants.
  auto nothrow_stage = [&]() noexcept -> std::error_code {
    // Update all in-memory data structures.
    phase2();

    // Now that all in-memory and all on-disk data structures have the commit,
    // we can update the in-memory completed-commit-ID.
    cm->completed_commit_id_ = seq_;
    cm->writes_.erase(cm->writes_.iterator_to(*this));

    // We no longer need to prevent other transactions from running.
    commit_lck.unlock();

    // Maybe pick up a new transaction.
    cm->maybe_start_front_write_locked_(wlck);
    return {};
  };
  return nothrow_stage();
}


} /* namespace monsoon::tx::detail */
