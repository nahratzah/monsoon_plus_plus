#ifndef MONSOON_OBJPIPE_DETAIL_INTERLOCKED_H
#define MONSOON_OBJPIPE_DETAIL_INTERLOCKED_H

///\file
///\ingroup objpipe_detail

#include <monsoon/objpipe/detail/reader_intf.h>
#include <monsoon/objpipe/detail/writer_intf.h>
#include <monsoon/objpipe/errc.h>
#include <mutex>
#include <memory>
#include <condition_variable>

namespace monsoon {
namespace objpipe {
namespace detail {


/**
 * \brief A threadsafe, interlocked objpipe.
 * \ingroup objpipe_detail
 *
 * An interlock objpipe is one where readers and writers synchronize the
 * hand-off of objects. Due to the synchronization, they can often eliminate
 * a move/copy operation, at the cost of additional synchronization.
 *
 * \tparam T \parblock
 * The type of objects in the objpipe.
 * \note The type may not be a reference, nor may it be const.
 * \endparblock
 * \sa \ref monsoon::objpipe::new_interlock()
 */
template<typename T>
class interlocked final
: public reader_intf<T>,
  public writer_intf<T>
{
  static_assert(!std::is_reference_v<T>,
      "Interlocked objpipe cannot deal with references.");
  static_assert(!std::is_const_v<T>,
      "Interlocked objpipe cannot deal with const objects.");

 public:
  ///@copydoc reader_intf<T>::value_type
  using value_type = T;
  ///@copydoc reader_intf<T>::pointer
  using pointer = std::add_pointer_t<value_type>;
  ///@copydoc writer_intf<T>::rvalue_reference
  using rvalue_reference = std::add_rvalue_reference_t<value_type>;

  ///@copydoc reader_intf<T>::is_pullable()
  auto is_pullable() const noexcept -> bool override {
    // offered_ can only be non-null if there is a writer.
    return this->has_writer();
  }

  ///@copydoc writer_intf<T>::is_pushable()
  auto is_pushable() const noexcept -> bool override {
    return this->has_reader();
  }

  ///@copydoc writer_intf<T>::push(rvalue_reference, objpipe_errc&)
  void push(rvalue_reference v, objpipe_errc& e) override {
    e = objpipe_errc::success;

    std::unique_lock<std::mutex> lck{ mtx_ };
    while (offered_ != nullptr) {
      write_avail_.wait(lck);
      if (!this->has_reader()) {
        e = objpipe_errc::closed;
        return;
      }
    }

    pointer v_addr = std::addressof(v);
    offered_ = v_addr;
    read_avail_.notify_one();
    continuation_notify_(continuation_, lck);
    write_done_.wait(lck, [this, v_addr]() { return offered_ != v_addr; });
  }

  ///@copydoc reader_intf<T>::wait()
  auto wait() const noexcept -> objpipe_errc override {
    std::unique_lock<std::mutex> lck{ mtx_ };
    while (offered_ == nullptr) {
      if (!this->has_writer()) return objpipe_errc::closed;
      read_avail_.wait(lck);
    }
    return objpipe_errc::success;
  }

  ///@copydoc reader_intf<T>::empty()
  auto empty() const noexcept -> bool override {
    std::lock_guard<std::mutex> lck{ mtx_ };
    return offered_ == nullptr;
  }

  ///@copydoc reader_intf<T>::pull(objpipe_errc&)
  auto pull(objpipe_errc& e) -> std::optional<value_type> override {
    e = objpipe_errc::success;

    std::unique_lock<std::mutex> lck{ mtx_ };
    while (offered_ == nullptr) {
      if (!this->has_writer()) {
        e = objpipe_errc::closed;
        return {};
      }
      read_avail_.wait(lck);
    }

    std::optional<value_type> result =
        std::make_optional<value_type>(std::move_if_noexcept(*std::exchange(offered_, nullptr)));
    lck.unlock();
    write_done_.notify_one();
    write_avail_.notify_one();
    return result;
  }

  ///@copydoc reader_intf<T>::pull()
  auto pull() -> value_type override {
    std::unique_lock<std::mutex> lck{ mtx_ };
    while (offered_ == nullptr) {
      if (!this->has_writer()) {
        throw std::system_error(
            static_cast<int>(objpipe_errc::closed),
            objpipe_category());
      }
      read_avail_.wait(lck);
    }

    value_type result = std::move_if_noexcept(*std::exchange(offered_, nullptr));
    lck.unlock();
    write_done_.notify_one();
    write_avail_.notify_one();
    return result;
  }

  ///@copydoc reader_intf<T>::try_pull(objpipe_errc&)
  auto try_pull(objpipe_errc& e) -> std::optional<value_type> override {
    e = objpipe_errc::success;

    std::unique_lock<std::mutex> lck{ mtx_ };
    if (offered_ == nullptr) {
      if (!this->has_writer())
        e = objpipe_errc::closed;
      return {};
    }

    std::optional<value_type> result =
        std::make_optional<value_type>(std::move_if_noexcept(*std::exchange(offered_, nullptr)));
    lck.unlock();
    write_done_.notify_one();
    write_avail_.notify_one();
    return result;
  }

  ///@copydoc reader_intf<T>::try_pull()
  auto try_pull() -> std::optional<value_type> override {
    std::unique_lock<std::mutex> lck{ mtx_ };
    if (offered_ == nullptr) return {};

    std::optional<value_type> result =
        std::make_optional<value_type>(std::move_if_noexcept(*std::exchange(offered_, nullptr)));
    lck.unlock();
    write_done_.notify_one();
    write_avail_.notify_one();
    return result;
  }

  ///@copydoc reader_intf<T>::front()
  auto front() const -> std::variant<pointer, objpipe_errc> override {
    using result_variant = std::variant<pointer, objpipe_errc>;

    std::unique_lock<std::mutex> lck{ mtx_ };
    while (offered_ == nullptr) {
      if (!this->has_writer())
        return result_variant(std::in_place_index<1>, objpipe_errc::closed);
      read_avail_.wait(lck);
    }

    return result_variant(std::in_place_index<0>, offered_);
  }

  ///@copydoc reader_intf<T>::pop_front()
  auto pop_front() -> objpipe_errc override {
    std::unique_lock<std::mutex> lck{ mtx_ };
    while (offered_ == nullptr) {
      if (!this->has_writer()) return objpipe_errc::closed;
      read_avail_.wait(lck);
    }

    offered_ = nullptr;
    lck.unlock();
    write_done_.notify_one();
    write_avail_.notify_one();
    return objpipe_errc::success;
  }

  ///@copydoc reader_intf<T>::add_continuation(std::unique_ptr<continuation_intf,writer_release>&&)
  void add_continuation(std::unique_ptr<continuation_intf, writer_release>&& c)
      override {
    std::lock_guard<std::mutex> lck{ mtx_ };

    // Note: by the end of the swap operation, the writers may have gone.
    // This is fine, as the last writer disappearing will invoke
    // on_last_writer_gone_(), which grabs the lck prior to releasing the
    // continuation_, which in this case (due to blocking) will be `c`.
    if (this->has_writer()) swap(continuation_, c);
  }

  ///@copydoc reader_intf<T>::erase_continuation(continuation_intf*)
  void erase_continuation(continuation_intf* c) override {
    std::unique_ptr<continuation_intf, writer_release> old_c;
    std::lock_guard<std::mutex> lck{ mtx_ };
    if (continuation_.get() == c) old_c = std::move(continuation_);
  }

 private:
  /**
   * When the last writer goes away:
   * - release the continuation
   *   (since interlocked is always empty if there are no writers)
   * - unblock all readers
   */
  void on_last_writer_gone_() noexcept override {
    std::unique_ptr<continuation_intf, writer_release> old_c;
    std::unique_lock<std::mutex> lck{ mtx_ };
    old_c = std::move(continuation_);
    // Implied: old_c::~unique_ptr(), after unlocking of lck;

    lck.unlock(); // Unlock prior to destruction of old_c pointee.

    // Signal any blocking readers.
    read_avail_.notify_all();
  }

  /**
   * When the last reader goes away:
   * - unblock all writers
   */
  void on_last_reader_gone_() noexcept override {
    // Signal any blocking writers.
    write_done_.notify_all();
    write_avail_.notify_all();
  }

  void continuation_notify_(
      std::unique_ptr<continuation_intf, writer_release>& continuation,
      std::unique_lock<std::mutex>& lck) noexcept {
    if (continuation != nullptr) {
      std::unique_ptr<continuation_intf, writer_release> live_continuation =
          writer_release::link(continuation_.get());
      lck.unlock();
      live_continuation->notify();
      live_continuation.reset();
      lck.lock();
    }
  }

  mutable std::mutex mtx_;
  mutable std::condition_variable read_avail_,
                                  write_avail_,
                                  write_done_;
  pointer offered_ = nullptr;
  std::unique_ptr<continuation_intf, writer_release> continuation_;
};


}}} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_INTERLOCKED_H */
