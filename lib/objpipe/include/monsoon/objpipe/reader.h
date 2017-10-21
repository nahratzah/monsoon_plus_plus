#ifndef MONSOON_OBJPIPE_READER_H
#define MONSOON_OBJPIPE_READER_H

///\file
///\ingroup objpipe

#include <cassert>
#include <memory>
#include <type_traits>
#include <monsoon/objpipe/errc.h>
#include <monsoon/objpipe/detail/reader_intf.h>
#include <monsoon/objpipe/detail/filter_operation.h>

namespace monsoon {
namespace objpipe {


/**
 * \brief An object pipe reader.
 * \ingroup objpipe
 *
 * \tparam T The type of objects emitted by the object pipe.
 */
template<typename T>
class reader {
 public:
  /** @brief The type of objects in this object pipe. */
  using value_type = T;
  /** @brief Reference type for objects in this object pipe. */
  using reference = std::add_lvalue_reference_t<value_type>;

  /**
   * \brief Construct a reader using the given pointer.
   *
   * Mainly used internally to the objpipe library.
   */
  explicit reader(std::unique_ptr<detail::reader_intf<T>, detail::reader_release> ptr) noexcept
  : ptr_(std::move(ptr))
  {}

  /**
   * @brief Test if the object pipe has data available.
   * @return true iff the objpipe has data available.
   */
  auto empty() const -> bool {
    assert(ptr_ != nullptr);
    return ptr_->empty();
  }

  /**
   * @brief Pull an object from the objpipe.
   *
   * @param[out] e An error condition used to communicate success/error reasons.
   * @return an initialized optional on success, empty optional on failure.
   */
  auto pull(objpipe_errc& e) -> std::optional<value_type> {
    assert(ptr_ != nullptr);
    return ptr_->pull(e);
  }

  /**
   * @brief Pull an object from the objpipe.
   *
   * @throw std::system_error if the pipe is empty and closed by the writer.
   */
  auto pull() -> value_type {
    assert(ptr_ != nullptr);
    return ptr_->pull();
  }

  /**
   * @brief Wait for the next element to become available.
   *
   * @return an enum indicating success or failure.
   */
  auto wait() const -> objpipe_errc {
    assert(ptr_ != nullptr);
    return ptr_->wait();
  }

  /**
   * @brief Yields a reference to the next element in the object pipe.
   *
   * @return a reference to the next element in the pipe.
   */
  auto front() const -> reference {
    assert(ptr_ != nullptr);
    auto f = ptr_->front();
    if (f.index() == 1u)
      throw std::system_error(static_cast<int>(std::get<1>(f)), objpipe_category());
    return *std::get<0u>(f);
  }

  /**
   * @brief Remove the next element from the object pipe.
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

  /**
   * @brief Apply a filter on the read elements.
   *
   * @param[in] pred A predicate.
   * @return A reader that only yields objects for which the predicate evaluates to true.
   */
  template<typename Pred>
  auto filter(Pred&& pred) && -> reader<T> {
    using filter_type = detail::filter_operation<T, std::decay_t<Pred>>;

    auto ptr = link(new filter_type(std::move(ptr_), std::forward<Pred>(pred)));
    return reader<T>(std::move(ptr));
  }

 private:
  std::unique_ptr<detail::reader_intf<T>, detail::reader_release> ptr_;
};

/**
 * \brief A shared object pipe reader.
 * \ingroup objpipe
 *
 * Multiple shared_reader instances may share the same object pipe.
 * An object pulled by any reader will not be read by any other reader.
 */
template<typename T>
class shared_reader {
 public:
  /** @brief The type of objects in this object pipe. */
  using value_type = T;
  /** @brief Reference type for objects in this object pipe. */
  using reference = std::add_lvalue_reference_t<value_type>;

  /**
   * @brief Pull an object from the objpipe.
   *
   * @param e An error condition used to communicate success/error reasons.
   * @return an initialized optional on success, empty optional on failure.
   */
  auto pull(objpipe_errc& e) -> std::optional<value_type> {
    assert(ptr_ != nullptr);
    return ptr_->pull(e);
  }

  /**
   * @brief Pull an object from the objpipe.
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
