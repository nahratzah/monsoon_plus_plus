#ifndef MONSOON_OBJPIPE_READER_H
#define MONSOON_OBJPIPE_READER_H

///@file monsoon/objpipe/reader.h <monsoon/objpipe/reader.h>

#include <cassert>
#include <memory>
#include <monsoon/objpipe/errc.h>
#include <monsoon/objpipe/detail/reader_intf.h>

namespace monsoon {
namespace objpipe {


/**
 * An object pipe reader.
 * @headerfile "" <monsoon/objpipe/reader.h>
 */
template<typename T>
class reader {
 public:
  /**
   * The type of objects in this object pipe.
   */
  using value_type = T;
  /**
   * Reference type for objects in this object pipe.
   */
  using reference = std::add_lvalue_reference_t<value_type>;

  /**
   * Test if the object pipe has data available.
   *
   * @return true iff the objpipe has data available.
   */
  auto empty() const -> bool {
    assert(ptr_ != nullptr);
    return ptr_->empty();
  }

  /**
   * Pull an object from the objpipe.
   *
   * @param[out] e An error condition used to communicate success/error reasons.
   * @return an initialized optional on success, empty optional on failure.
   */
  auto pull(objpipe_errc& e) -> std::optional<value_type> {
    assert(ptr_ != nullptr);
    return ptr_->pull(e);
  }

  /**
   * Pull an object from the objpipe.
   *
   * @throw std::system_error if the pipe is empty and closed by the writer.
   */
  auto pull() -> value_type {
    assert(ptr_ != nullptr);
    return ptr_->pull();
  }

  /**
   * Wait for the next element to become available.
   *
   * @return an enum indicating success or failure.
   */
  auto wait() const -> objpipe_errc {
    assert(ptr_ != nullptr);
    return ptr_->wait();
  }

  /**
   * Yields a reference to the next element in the object pipe.
   *
   * @return a reference to the next element in the pipe.
   */
  auto front() const -> reference {
    assert(ptr_ != nullptr);
    return std::visit(
        [](auto&& v) {
          if constexpr(std::is_same_t<ojbpipe_errc, std::decay_t<decltype(v)>>)
            throw std::system_error(static_cast<int>(v), objpipe_category());
          else
            return std::forward<std::decay_t<decltype(v)>>(v);
        },
        ptr_->front());
  }

  /**
   * Remove the next element from the object pipe.
   */
  void pop_front() {
    assert(ptr_ != nullptr);
    ptr_->pop_front();
  }

  /**
   * @return true iff the reader is valid and pullable.
   */
  explicit operator bool() const noexcept {
    return ptr_ != nullptr && ptr_->is_pullable();
  }

 private:
  std::unique_ptr<detail::reader_intf<T>, detail::reader_release> ptr_;
};

/**
 * A shared object pipe reader.
 *
 * Multiple shared_reader instances may share the same object pipe.
 * An object pulled by any reader will not be read by any other reader.
 */
template<typename T>
class shared_reader {
 public:
  /**
   * Pull an object from the objpipe.
   *
   * @param e An error condition used to communicate success/error reasons.
   * @return an initialized optional on success, empty optional on failure.
   */
  auto pull(objpipe_errc& e) -> std::optional<value_type> {
    assert(ptr_ != nullptr);
    return ptr_->pull(e);
  }

  /**
   * Pull an object from the objpipe.
   *
   * @throw std::system_error if the pipe is empty and closed by the writer.
   */
  auto pull() -> value_type {
    assert(ptr_ != nullptr);
    return ptr_->pull();
  }

  /**
   * @return true iff the reader is valid and pullable.
   */
  explicit operator bool() const noexcept {
    return ptr_ != nullptr && ptr_->is_pullable();
  }

 private:
  std::unique_ptr<detail::reader_intf<T>, detail::reader_release> ptr_;
};


}} /* namespace monsoon::objpipe */

#endif /* MONSOON_OBJPIPE_READER_H */
