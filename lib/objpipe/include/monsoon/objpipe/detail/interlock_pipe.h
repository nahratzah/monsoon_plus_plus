#ifndef MONSOON_OBJPIPE_DETAIL_INTERLOCK_PIPE_H
#define MONSOON_OBJPIPE_DETAIL_INTERLOCK_PIPE_H

#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <type_traits>
#include <utility>
#include <monsoon/objpipe/errc.h>
#include <monsoon/objpipe/detail/transport.h>

namespace monsoon::objpipe::detail {


template<typename T>
class interlock_impl {
 private:
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  using front_type = std::conditional_t<std::is_const_v<T>,
        std::add_lvalue_reference_t<T>,
        std::add_rvalue_reference_t<T>>;
  using pull_type = value_type;

 public:
  auto is_pullable()
  -> bool {
    std::lock_guard<std::mutex> lck_{ guard_ };
    return offered_ != nullptr || writer_count_ > 0;
  }

  auto wait()
  -> objpipe_errc {
    std::unique_lock<std::mutex> lck_{ guard_ };
    for (;;) {
      if (offered_ != nullptr) return objpipe_errc::success;
      if (writer_count_ == 0) return objpipe_errc::closed;
      read_ready_.wait(lck_);
    }
  }

  auto front()
  -> transport<front_type> {
    std::unique_lock<std::mutex> lck_{ guard_ };
    for (;;) {
      if (offered_ != nullptr)
        return transport<front_type>(std::in_place_index<0>, get(lck_));
      if (writer_count_ == 0)
        return transport<front_type>(std::in_place_index<1>, objpipe_errc::closed);
      read_ready_.wait(lck_);
    }
  }

  auto pop_front()
  -> objpipe_errc {
    std::unique_lock<std::mutex> lck_{ guard_ };
    while (offered_ == nullptr) {
      if (writer_count_ == 0) return objpipe_errc::closed;
      read_ready_.wait(lck_);
    }

    offered_ = nullptr;
    lck_.unlock();
    write_ready_.notify_one();
    return objpipe_errc::success;
  }

  auto pull()
  -> transport<pull_type> {
    std::unique_lock<std::mutex> lck_{ guard_ };
    while (offered_ == nullptr) {
      if (writer_count_ == 0)
        return transport<pull_type>(std::in_place_index<1>, objpipe_errc::closed);
      read_ready_.wait(lck_);
    }

    auto result = transport<pull_type>(std::in_place_index<0>, get(lck_));
    offered_ = nullptr;
    lck_.unlock();
    write_ready_.notify_one();
    return result;
  }

  auto try_pull()
  -> transport<pull_type> {
    std::unique_lock<std::mutex> lck_{ guard_ };
    if (offered_ == nullptr) {
      if (writer_count_ == 0)
        return transport<pull_type>(std::in_place_index<1>, objpipe_errc::closed);
      else
        return transport<pull_type>(std::in_place_index<1>, objpipe_errc::success);
    }

    auto result = transport<pull_type>(std::in_place_index<0>, get(lck_));
    offered_ = nullptr;
    lck_.unlock();
    write_ready_.notify_one();
    return result;
  }

  template<bool Enable = std::is_const_v<T>>
  auto publish(std::conditional_t<std::is_const_v<T>,
      std::add_lvalue_reference_t<std::add_const_t<value_type>>,
      std::add_rvalue_reference_t<value_type>> v)
  -> objpipe_errc {
    std::unique_lock<std::mutex> lck_{ guard_ };
    write_ready_.wait(lck_,
        [this]() {
          return offered_ == nullptr || reader_count_ == 0;
        });
    if (reader_count_ == 0) return objpipe_errc::closed;

    std::add_pointer_t<T> v_ptr = nullptr;
    if constexpr(std::is_const_v<T>)
      v_ptr = std::addressof(v);
    else
      v_ptr = std::addressof(static_cast<std::add_lvalue_reference_t<value_type>>(v));
    offered_ = v_ptr;
    read_ready_.notify_one();
    write_ready_.wait(lck_,
        [this, v_ptr]() {
          return offered_ != v_ptr || reader_count_ == 0;
        });

    if (offered_ != v_ptr) return objpipe_errc::success;
    offered_ = nullptr;
    assert(reader_count_ == 0);
    return objpipe_errc::closed;
  }

  template<bool Enable = !std::is_const_v<T>>
  auto publish(std::add_lvalue_reference_t<std::add_const_t<value_type>> v)
  -> std::enable_if_t<Enable, objpipe_errc> {
    return publish(T(v));
  }

  auto inc_reader() -> void {
    std::lock_guard<std::mutex> lck{ guard_ };
    ++reader_count_;
    assert(reader_count_ > 0);
  }

  auto subtract_reader() -> bool {
    std::lock_guard<std::mutex> lck{ guard_ };
    assert(reader_count_ > 0);
    if (--reader_count_ == 0) write_ready_.notify_all();
    return reader_count_ == 0 && writer_count_ == 0;
  }

  auto inc_writer() -> void {
    std::lock_guard<std::mutex> lck{ guard_ };
    ++writer_count_;
    assert(writer_count_ > 0);
  }

  auto subtract_writer() -> bool {
    std::lock_guard<std::mutex> lck{ guard_ };
    assert(writer_count_ > 0);
    if (--writer_count_ == 0) read_ready_.notify_all();
    return reader_count_ == 0 && writer_count_ == 0;
  }

 private:
  auto get(const std::unique_lock<std::mutex>& lck) const
  noexcept
  -> std::conditional_t<std::is_const_v<T>,
      std::add_lvalue_reference_t<std::add_const_t<value_type>>,
      std::add_rvalue_reference_t<value_type>> {
    assert(lck.owns_lock() && lck.mutex() == &guard_);
    assert(offered_ != nullptr);
    if constexpr(std::is_const_v<T>)
      return *offered_;
    else
      return std::move(*offered_);
  }

  std::mutex guard_;
  std::condition_variable read_ready_;
  std::condition_variable write_ready_;
  std::add_pointer_t<T> offered_ = nullptr;
  std::uintptr_t writer_count_ = 0;
  std::uintptr_t reader_count_ = 0;
};

template<typename T>
class interlock_pipe {
 public:
  interlock_pipe() = default;

  explicit interlock_pipe(interlock_impl<T>* ptr)
  noexcept
  : ptr_(ptr)
  {
    if (ptr_ != nullptr) ptr_->inc_reader();
  }

  interlock_pipe(interlock_pipe&& other)
  noexcept
  : ptr_(std::exchange(other.ptr_, nullptr))
  {}

  auto operator=(interlock_pipe&& other)
  noexcept
  -> interlock_pipe& {
    using std::swap;
    swap(ptr_, other.ptr_);
    return *this;
  }

  interlock_pipe(const interlock_pipe& other) = delete;
  auto operator=(const interlock_pipe& other) -> interlock_pipe& = delete;
#if 0 // Can't copy, due to front() and pop_front() combination not being multiple-reader proof.
  interlock_pipe(const interlock_pipe& other)
  noexcept
  : ptr_(other.ptr_)
  {
    if (ptr_ != nullptr) ptr_->inc_reader();
  }

  auto operator=(const interlock_pipe& other)
  noexcept
  -> interlock_pipe& {
    return *this = interlock_pipe(other);
  }
#endif

  ~interlock_pipe()
  noexcept {
    if (ptr_ != nullptr && ptr_->subtract_reader())
      delete ptr_;
  }

  auto is_pullable() const
  -> bool {
    assert(ptr_ != nullptr);
    return ptr_->is_pullable();
  }

  auto wait() const
  -> objpipe_errc {
    assert(ptr_ != nullptr);
    return ptr_->wait();
  }

  auto front() const
  -> decltype(auto) {
    assert(ptr_ != nullptr);
    return ptr_->front();
  }

  auto pop_front() const
  -> decltype(auto) {
    assert(ptr_ != nullptr);
    return ptr_->pop_front();
  }

  auto pull() const
  -> decltype(auto) {
    assert(ptr_ != nullptr);
    return ptr_->pull();
  }

  auto try_pull() const
  -> decltype(auto) {
    assert(ptr_ != nullptr);
    return ptr_->try_pull();
  }

 private:
  interlock_impl<T>* ptr_ = nullptr;
};

template<typename T>
class interlock_writer {
 public:
  interlock_writer() = default;

  explicit interlock_writer(interlock_impl<T>* ptr)
  noexcept
  : ptr_(ptr)
  {
    if (ptr_ != nullptr) ptr_->inc_writer();
  }

  interlock_writer(interlock_writer&& other)
  noexcept
  : ptr_(std::exchange(other.ptr_, nullptr))
  {}

  interlock_writer(const interlock_writer& other)
  noexcept
  : ptr_(other.ptr_)
  {
    if (ptr_ != nullptr) ptr_->inc_writer();
  }

  auto operator=(const interlock_writer& other)
  noexcept
  -> interlock_writer& {
    return *this = interlock_writer(other);
  }

  auto operator=(interlock_writer&& other)
  noexcept
  -> interlock_writer& {
    using std::swap;
    swap(ptr_, other.ptr_);
    return *this;
  }

  ~interlock_writer()
  noexcept {
    if (ptr_ != nullptr && ptr_->subtract_writer())
      delete ptr_;
  }

  template<typename Arg>
  auto operator()(Arg&& arg)
  -> void {
    assert(ptr_ != nullptr);
    objpipe_errc e = ptr_->publish(std::forward<Arg>(arg));
    if (e != objpipe_errc::success) {
      throw std::system_error(
          static_cast<int>(e),
          objpipe_category());
    }
  }

  template<typename Arg>
  auto operator()(Arg&& arg, objpipe_errc& e)
  -> void {
    assert(ptr_ != nullptr);
    e = ptr_->publish(std::forward<Arg>(arg));
  }

 private:
  interlock_impl<T>* ptr_ = nullptr;
};


} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_INTERLOCK_PIPE_H */
