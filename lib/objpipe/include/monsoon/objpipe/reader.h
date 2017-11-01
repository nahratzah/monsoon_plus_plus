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
#include <monsoon/objpipe/detail/map_operation.h>

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
  /** \brief The type of objects in this object pipe. */
  using value_type = std::decay_t<T>;
  /** \brief Reference type for objects in this object pipe. */
  using reference = std::add_lvalue_reference_t<value_type>;

  ///@{
  class iterator;

  iterator begin() {
    return iterator(*this);
  }

  iterator end() {
    return iterator();
  }
  ///@}

  /**
   * \brief Construct a reader using the given pointer.
   *
   * Mainly used internally to the objpipe library.
   */
  explicit reader(std::unique_ptr<detail::reader_intf<T>, detail::reader_release> ptr) noexcept
  : ptr_(std::move(ptr))
  {}

  /**
   * \brief Test if the object pipe has data available.
   * \return true iff the objpipe has data available.
   */
  auto empty() const -> bool {
    assert(ptr_ != nullptr);
    return ptr_->empty();
  }

  /**
   * \brief Pull an object from the objpipe.
   *
   * \param[out] e An error condition used to communicate success/error reasons.
   * \return an initialized optional on success, empty optional on failure.
   */
  auto pull(objpipe_errc& e) -> std::optional<value_type> {
    assert(ptr_ != nullptr);
    return ptr_->pull(e);
  }

  /**
   * \brief Pull an object from the objpipe.
   *
   * \throw std::system_error if the pipe is empty and closed by the writer.
   */
  auto pull() -> value_type {
    assert(ptr_ != nullptr);
    return ptr_->pull();
  }

  /**
   * \brief Pull an object from the objpipe.
   *
   * \return a pulled value, if one is available without waiting;
   *   an empty optional otherwise.
   */
  auto try_pull() -> std::optional<value_type> {
    assert(ptr_ != nullptr);
    return ptr_->try_pull();
  }

  /**
   * \brief Wait for the next element to become available.
   *
   * \return an enum indicating success or failure.
   */
  auto wait() const -> objpipe_errc {
    assert(ptr_ != nullptr);
    return ptr_->wait();
  }

  /**
   * \brief Yields a reference to the next element in the object pipe.
   *
   * \return a reference to the next element in the pipe.
   */
  auto front() const -> reference {
    assert(ptr_ != nullptr);
    auto f = ptr_->front();
    if (f.index() == 1u)
      throw std::system_error(static_cast<int>(std::get<1>(f)), objpipe_category());
    return *std::get<0u>(f);
  }

  /**
   * \brief Remove the next element from the object pipe.
   */
  void pop_front() {
    assert(ptr_ != nullptr);
    ptr_->pop_front();
  }

  /**
   * \return true iff the reader is valid and pullable.
   */
  explicit operator bool() const noexcept {
    return ptr_ != nullptr && ptr_->is_pullable();
  }

  /**
   * \brief Apply a filter on the read elements.
   *
   * \param[in] pred A predicate.
   * \return A reader that only yields objects for which the predicate evaluates to true.
   */
  template<typename Pred>
  auto filter(Pred&& pred) && -> reader<T> {
    using operation_type = detail::filter_operation<T, std::decay_t<Pred>>;

    auto ptr = detail::reader_release::link(
        new operation_type(std::move(ptr_), std::forward<Pred>(pred)));
    return reader<T>(std::move(ptr));
  }

  /**
   * \brief Apply a transformation on the read elements.
   *
   * \param[in] fn A unary invokable to be applied to each element.
   * \return A reader that invokes \p fn on each element, yielding the result.
   */
  template<typename Fn>
  auto transform(Fn&& fn) && -> reader<std::decay_t<detail::map_out_type<T, Fn>>> {
    using result_type = detail::map_out_type<T, std::decay_t<Fn>>;
    using operation_type = detail::map_operation<T, std::decay_t<Fn>, result_type>;

    auto ptr = detail::reader_release::link(
        new operation_type(std::move(ptr_), std::forward<Fn>(fn)));
    return reader<std::decay_t<result_type>>(std::move(ptr));
  }

  /**
   * \brief Apply a transformation on the read elements.
   *
   * The transformed value will be copied.
   *
   * \param[in] fn A unary invokable to be applied to each element.
   * \return A reader that invokes \p fn on each element, yielding the result.
   */
  template<typename Fn>
  auto transform_copy(Fn&& fn) && -> reader<std::decay_t<detail::map_out_type<T, Fn>>> {
    using result_type = std::decay_t<detail::map_out_type<T, std::decay_t<Fn>>>;
    using operation_type = detail::map_operation<T, std::decay_t<Fn>, result_type>;

    auto ptr = detail::reader_release::link(
        new operation_type(std::move(ptr_), std::forward<Fn>(fn)));
    return reader<result_type>(std::move(ptr));
  }

  /**
   * \brief Apply functor on each pulled value.
   *
   * \param[in] fn A unary invokable to be applied to each element.
   * \return \p fn
   */
  template<typename Fn>
  Fn for_each(Fn fn) && {
    objpipe_errc e;
    for (std::optional<value_type> v = pull(e);
        e == objpipe_errc::success;
        v = pull(e)) {
      assert(v.has_value());
      std::invoke(fn, std::move(v.value()));
    }
    return fn;
  }

 private:
  std::unique_ptr<detail::reader_intf<T>, detail::reader_release> ptr_;
};

/**
 * \brief Iterate over all elements in the reader.
 * \ingroup objpipe
 *
 * This is an input iterator.
 */
template<typename T>
class reader<T>::iterator {
  friend class reader<T>;

 public:
  ///\copydoc reader<T>::value_type
  using value_type = typename reader<T>::value_type;
  ///\brief The reference type is the value type.
  using reference = value_type;
  ///\brief We do not supply pointers.
  using pointer = void;
  ///\brief Difference type.
  using difference_type = ptrdiff_t;
  ///\brief The iterator is an input iterator.
  using iterator_category = std::input_iterator_tag;

  iterator() = default;

 private:
  explicit iterator(reader<T>& r) noexcept
  : reader_(&r)
  {}

 public:
  /**
   * \brief Dereference the iterator.
   *
   * This is an input iterator, therefore the dereference operation
   * must be called exactly once between increments.
   * \return the value type of the underlying objpipe.
   * \throw std::system_error if the objpipe cannot be pulled from.
   */
  auto operator*() -> reference {
    assert(reader_ != nullptr && bool(*reader_));
    return reader_->pull();
  }

  /**
   * \brief Advance iterator.
   *
   * This is effectively a noop, as the input iterator advances at dereference.
   * \return the iterator.
   */
  auto operator++() -> iterator& {
    return *this;
  }

  /**
   * \brief Advance iterator (postfix increment operator).
   *
   * This is effectively a noop, as the input iterator advances at dereference.
   */
  auto operator++(int) -> void {
    ++*this;
  }

  /**
   * \brief Check if two objpipe iterators are equal.
   *
   * Two iterators are considered equal if both are end iterators.
   * All other iterators are considered not equal.
   * \param x,y The iterators to check for equality.
   * \note Only end iterators compare as equal.
   */
  friend bool operator==(const iterator& x, const iterator& y) noexcept {
    if (x.reader_ != nullptr
        && x.reader_->wait() != objpipe_errc::closed)
      return false;

    if (y.reader_ != nullptr
        && y.reader_->wait() != objpipe_errc::closed)
      return false;

    // Both are at the end.
    return true;
  }

  /**
   * \brief Check if two objpipe iterators are not equal.
   *
   * Two iterators are considered equal if both are end iterators.
   * All other iterators are considered not equal.
   * \param x,y The iterators to check for equality.
   * \note Only end iterators compare as equal.
   */
  friend bool operator!=(const iterator& x, const iterator& y) noexcept {
    return !(x == y);
  }

 private:
  reader<T>* reader_ = nullptr;
};


}} /* namespace monsoon::objpipe */

#endif /* MONSOON_OBJPIPE_READER_H */
