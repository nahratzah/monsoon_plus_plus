#ifndef MONSOON_OBJPIPE_DETAIL_ADAPTER_H
#define MONSOON_OBJPIPE_DETAIL_ADAPTER_H

///\file
///\ingroup objpipe_detail

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>
#include <monsoon/objpipe/reader.h>
#include <monsoon/objpipe/detail/fwd.h>
#include <monsoon/objpipe/detail/adapt.h>
#include <monsoon/objpipe/detail/filter_op.h>
#include <monsoon/objpipe/detail/flatten_op.h>
#include <monsoon/objpipe/detail/peek_op.h>
#include <monsoon/objpipe/detail/transform_op.h>
#include <monsoon/objpipe/detail/deref_op.h>
#include <monsoon/objpipe/detail/select_op.h>
#include <monsoon/objpipe/detail/virtual.h>
#include <monsoon/objpipe/detail/transport.h>

namespace monsoon::objpipe::detail {


template<typename Source>
constexpr auto adapter(Source&& src)
noexcept(std::is_nothrow_constructible_v<adapter_t<std::decay_t<Source>>, Source>)
-> adapter_t<std::decay_t<Source>> {
  return adapter_t<std::decay_t<Source>>(std::forward<Source>(src));
}

///\brief Store a value of T, using either a copy or a pointer.
///\ingroup objpipe_detail
template<typename T, bool UsePointer>
class front_store_handler_data_;

template<typename T>
class front_store_handler_data_<T, true> {
 public:
  auto get() const
  noexcept
  -> std::add_lvalue_reference_t<T> {
    assert(ptr_ != nullptr);
    return *ptr_;
  }

  template<typename Source>
  auto get_or_assign(Source& src) const
  -> std::add_lvalue_reference_t<T> {
    if (ptr_ == nullptr) {
      static_assert(std::is_reference_v<decltype(adapt::front(src))>,
          "Programmer error: front() returns a temporary, instead of a reference.");

      ptr_ = addressof_(adapt::front(src));
    }
    return *ptr_;
  }

  auto release() const
  noexcept
  -> std::add_rvalue_reference_t<T> {
    assert(ptr_ != nullptr);
    return std::move(*std::exchange(ptr_, nullptr));
  }

  auto present() const
  noexcept
  -> bool {
    return ptr_ != nullptr;
  }

  auto reset() const
  noexcept
  -> bool {
    return std::exchange(ptr_, nullptr) != nullptr;
  }

 private:
  static auto addressof_(T&& arg)
  -> std::add_pointer_t<T> {
    return std::addressof(arg);
  }

  static auto addressof_(T& arg)
  -> std::add_pointer_t<T> {
    return std::addressof(arg);
  }

  mutable std::add_pointer_t<T> ptr_ = nullptr;
};

template<typename T>
class front_store_handler_data_<T, false> {
 public:
  auto get() const
  noexcept
  -> std::add_lvalue_reference_t<T> {
    assert(val_.has_value());
    return *val_;
  }

  template<typename Source>
  auto get_or_assign(Source& src) const
  -> std::add_lvalue_reference_t<T> {
    if (!val_.has_value())
      val_.emplace(adapt::front(src));
    return *val_;
  }

  auto release() const
  noexcept(std::is_nothrow_move_constructible_v<T>)
  -> T {
    assert(val_.has_value());
    T rv = *std::move(val_);
    val_.reset();
    return rv;
  }

  auto present() const
  noexcept
  -> bool {
    return val_.has_value();
  }

  auto reset() const
  noexcept(noexcept(val_.reset()))
  -> bool {
    bool rv = val_.has_value();
    val_.reset();
    return rv;
  }

 private:
  mutable std::optional<T> val_{};
};

template<typename Source>
struct front_store_handler_ {
 private:
  using src_front_type = adapt::front_type<Source>;

  static constexpr bool use_ref = !std::is_const_v<src_front_type>
      && !std::is_volatile_v<src_front_type>
      && (std::is_lvalue_reference_v<src_front_type>
          || std::is_rvalue_reference_v<src_front_type>);

  using store_type = adapt::value_type<Source>;

 public:
  using type = front_store_handler_data_<store_type, use_ref>;
};

///\brief Decide on which store handler (copy or pointer) to use, based on Source.
///\ingroup objpipe_detail
template<typename Source>
using front_store_handler = typename front_store_handler_<Source>::type;


/**
 * \brief Iterator for source.
 * \ingroup objpipe_detail
 *
 * \details
 * An input iterator, iterating over the elements of the given objpipe.
 */
template<typename Source>
class adapter_iterator {
 public:
  using value_type = adapt::value_type<Source>;
  using reference = adapt::pull_type<Source>;
  using difference_type = std::ptrdiff_t;
  using pointer = void;
  using iterator_category = std::input_iterator_tag;

  adapter_iterator() = default;

  constexpr adapter_iterator(Source& src)
  noexcept
  : src_(std::addressof(src))
  {}

  auto operator*() const
  -> reference {
    return adapt::pull(*src_);
  }

  auto operator++()
  -> adapter_iterator& {
    return *this;
  }

  auto operator++(int)
  noexcept
  -> void {
    ++*this;
  }

  constexpr auto operator==(const adapter_iterator& other) const
  noexcept
  -> bool {
    return (src_ == nullptr || src_->wait() == objpipe_errc::closed)
        && (other.src_ == nullptr || other.src_->wait() == objpipe_errc::closed);
  }

  constexpr auto operator!=(const adapter_iterator& other) const
  noexcept
  -> bool {
    return !(*this == other);
  }

 private:
  Source* src_ = nullptr;
};


/**
 * \brief Adapt a source to have all the objpipe functions.
 * \ingroup objpipe_detail
 *
 * \details
 * This class basically provides the throwing versions of each method.
 * It also applies transformations and handles various algorithms.
 *
 * \tparam Source The source that is to be wrapped.
 * \sa \ref monsoon::objpipe::reader
 */
template<typename Source>
class adapter_t {
  static_assert(std::is_move_constructible_v<Source>,
      "Source must be move constructible.");

 public:
  using value_type = adapt::value_type<Source>;
  using iterator = adapter_iterator<Source>;

  constexpr adapter_t() = default;

  ///\brief Constructor, wraps the given source.
  explicit constexpr adapter_t(Source&& src)
  noexcept(std::is_nothrow_move_constructible_v<Source>)
  : src_(std::move(src))
  {}

  /**
   * \brief Tests if the objpipe is pullable.
   *
   * \details
   * An objpipe is pullable if it has remaining elements.
   *
   * Contrary to empty() or wait(), a pullable objpipe may still fail to yield a value.
   * This because the objpipe is also considered pullable if it has a writer attached,
   * which may detach between testing is_pullable and retrieval of a value.
   * In this scenario, the value retrieval would fail, due to the writer not having
   * supplied any.
   *
   * Best used to decide if an objpipe is ready for discarding:
   * false will be returned iff the objpipe has no pending elements and no
   * writers that can add future pending elements.
   *
   * \return False if the objpipe is empty and has no writers. True otherwise.
   */
  auto is_pullable()
  noexcept
  -> bool {
    return adapt::is_pullable(const_cast<Source&>(src_));
  }

  /**
   * \brief Wait until a new value is available.
   *
   * \details
   * If wait returns successfully, \ref try_pull() is certain not to yield an empty optional.
   *
   * \return An \ref monsoon::ojbpipe::errc "objpipe_errc" indicating if the call succeeded.
   */
  auto wait() const
  noexcept(noexcept(adapt::wait(std::declval<Source&>())))
  -> objpipe_errc {
    return adapt::wait(const_cast<Source&>(src_));
  }

  /**
   * \brief Test if the objpipe is empty.
   *
   * \return True if the objpipe is empty, false if the objpipe has a pending value.
   */
  auto empty() const
  noexcept(noexcept(std::declval<Source&>().wait()))
  -> bool {
    const objpipe_errc e = const_cast<Source&>(src_).wait();
    return e == objpipe_errc::closed;
  }

  /**
   * \brief Retrieve the next value in the objpipe, without advancing.
   *
   * \details
   * Retrieves the next value in the objpipe.
   *
   * Front will yield the same element, until \ref pop_front() or \ref pull() has been invoked.
   *
   * \return The next value in the objpipe.
   * \throw std::system_error if the objpipe has no values.
   */
  auto front() const
  -> std::add_lvalue_reference_t<value_type> {
    return store_.get_or_assign(const_cast<Source&>(src_));
  }

  /**
   * \brief Remove the next value from the objpipe.
   *
   * \details
   * This function advances the objpipe without returning the value.
   */
  auto pop_front()
  -> void {
    adapt::pop_front(src_);
    store_.reset();
  }

  /**
   * \brief Try pulling a value from the objpipe.
   *
   * \details
   * Pulling removes a value from the objpipe, returning it.
   *
   * \return The next value in the objpipe, or an empty optional if the value is not (yet) available.
   */
  auto try_pull()
  -> std::optional<value_type> {
    if (store_.present()) {
      auto rv = std::optional<value_type>(store_.release());
      adapt::pop_front(src_);
      return rv;
    } else {
      return adapt::try_pull(src_);
    }
  }

  /**
   * \brief Pull a value from the objpipe.
   *
   * \details
   * Pulling removes a value from the objpipe, returning it.
   *
   * \return The next value in the objpipe.
   * \throw std::system_error if the objpipe has no remaining values.
   */
  auto pull()
  -> value_type {
    if (store_.present()) {
      value_type rv = store_.release();
      adapt::pop_front(src_);
      return rv;
    } else {
      return adapt::pull(src_);
    }
  }

  /**
   * \brief Pull a value from the objpipe.
   *
   * \details
   * Pulling removes a value from the objpipe, returning it.
   *
   * If the pull fails, the error code argument will be set to indicate.
   * Otherwise, the error code argument will be set to objpipe_errc::success.
   *
   * \return The next value in the objpipe, if one is available.
   */
  auto pull(objpipe_errc& e)
  noexcept(std::is_nothrow_move_constructible_v<value_type>
      && std::is_nothrow_destructible_v<value_type>
      && noexcept(std::declval<front_store_handler<Source>&>().present())
      && noexcept(std::declval<front_store_handler<Source>&>().release())
      && noexcept(adapt::pull(std::declval<Source&>(), std::declval<objpipe_errc&>())))
  -> std::optional<value_type> {
    if (store_.present()) {
      auto rv = std::optional<value_type>(store_.release());
      e = src_.pop_front();
      if (e != objpipe_errc::success)
        rv.reset();
      return rv;
    }

    return adapt::pull(src_, e);
  }

  /**
   * \brief Filter values in the objpipe.
   *
   * \details
   * The objpipe values omitted, unless the predicate returns true.
   *
   * \param[in] pred A predicate to invoke on each element.
   * \return An objpipe, with only elements for which the predicate yields true.
   */
  template<typename Pred>
  auto filter(Pred&& pred) &&
  noexcept(noexcept(adapter(adapt::filter(std::declval<Source>(), std::forward<Pred>(pred)))))
  -> decltype(auto) {
    return adapter(adapt::filter(std::move(src_), std::forward<Pred>(pred)));
  }

  /**
   * \brief Functional transformation of each element.
   *
   * \details
   * The functor is invoked for each element, and the elements are replaced by the result of the functor.
   *
   * The functor does the right thing when a reference is returned.
   *
   * \note When returning a reference, the value of the reference may be
   * mutated by subsequent transformations in the objpipe, unless the reference
   * is const.
   *
   * \param[in] fn Functor that is invoked for each element.
   * \return An objpipe, where each value is replaced by the result of the \p fn invocation.
   */
  template<typename Fn>
  auto transform(Fn&& fn) &&
  noexcept(noexcept(adapter(adapt::transform(std::declval<Source>(), std::forward<Fn>(fn)))))
  -> decltype(auto) {
    return adapter(adapt::transform(std::move(src_), std::forward<Fn>(fn)));
  }

  /**
   * \brief Transform each element by dereferencing it.
   *
   * \details
   * Invokes
   * \code
   * *element
   * \endcode
   * for each element in the objpipe.
   *
   * \returns An objpipe of the dereferenced elements from the original objpipe.
   */
  template<bool Enable = deref_op::is_valid<value_type>>
  auto deref() &&
  noexcept(noexcept(std::declval<adapter_t>().transform(deref_op())))
  -> decltype(auto) {
    return std::move(*this).transform(deref_op());
  }

  /**
   * \brief Transform each element by invoking std::get<Idx>.
   * \returns An objpipe of elements selected using the given \p Index.
   */
  template<size_t Index>
  auto select() &&
  noexcept(noexcept(std::declval<adapter_t>().transform(
              std::enable_if_t<
                  select_index_op<Index>::template is_valid<value_type>,
                  select_index_op<Index>>())))
  -> decltype(std::declval<adapter_t>().transform(
          std::enable_if_t<
              select_index_op<Index>::template is_valid<value_type>,
              select_index_op<Index>>())) {
    return std::move(*this).transform(select_index_op<Index>());
  }

  /**
   * \brief Transform each element by invoking std::get<Type>.
   * \returns An objpipe of elements selected using the given \p Type.
   */
  template<typename Type>
  auto select() &&
  noexcept(noexcept(std::declval<adapter_t>().transform(
              std::enable_if_t<
                  select_type_op<Type>::template is_valid<value_type>,
                  select_type_op<Type>>())))
  -> decltype(std::declval<adapter_t>().transform(
          std::enable_if_t<
              select_type_op<Type>::template is_valid<value_type>,
              select_type_op<Type>>())) {
    return std::move(*this).transform(select_type_op<Type>());
  }

  /**
   * \brief Transform each element pair by selecting its first member.
   * \returns An objpipe of the first member-variable of elements in the original objpipe.
   */
  template<bool Enable = select_first_op::is_valid<value_type>>
  auto select_first() &&
  noexcept(noexcept(std::declval<adapter_t>().transform(
              std::enable_if_t<Enable, select_first_op>())))
  -> decltype(std::declval<adapter_t>().transform(
          std::enable_if_t<Enable, select_first_op>())) {
    return std::move(*this).transform(select_first_op());
  }

  /**
   * \brief Transform each element pair by selecting its second member.
   * \returns An objpipe of the second member-variable of elements in the original objpipe.
   */
  template<bool Enable = select_second_op::is_valid<value_type>>
  auto select_second() &&
  noexcept(noexcept(std::declval<adapter_t>().transform(
              std::enable_if_t<Enable, select_second_op>())))
  -> decltype(std::declval<adapter_t>().transform(
          std::enable_if_t<Enable, select_second_op>())) {
    return std::move(*this).transform(select_second_op());
  }

  /**
   * \brief Mutate or inspect values as they are passed through the objpipe.
   *
   * \details
   *
   * \param[in] fn A functor that is invoked for each element.
   * The functor is allowed, but not required to, modify the element in place.
   * \return An objpipe, where \p fn is invoked for each element.
   */
  template<typename Fn>
  auto peek(Fn&& fn) &&
  noexcept(noexcept(adapter(adapt::peek(std::declval<Source>(), std::forward<Fn>(fn)))))
  -> decltype(auto) {
    return adapter(adapt::peek(std::move(src_), std::forward<Fn>(fn)));
  }

  /**
   * \brief Assert that the given predicate holds for values in the objpipe.
   *
   * \param[in] fn A predicate returning a boolean.
   * \return The same objpipe.
   */
  template<typename Fn>
  auto assertion(Fn&& fn) &&
  noexcept(noexcept(adapter(adapt::assertion(std::declval<Source>(), std::forward<Fn>(fn)))))
  -> decltype(auto) {
    return adapter(adapt::assertion(std::move(src_), std::forward<Fn>(fn)));
  }

  /**
   * \brief Transformation operation that replaces each iterable element in the objpipe with its elements.
   *
   * \details
   * If the objpipe holds a type that has an iterator begin() and end() function,
   * the operation will replace the collection with the values in it.
   *
   * Ojbpipes, as well as all the STL collection types, are iterable.
   *
   * \return An objpipe where each collection element is replaced with its elements.
   */
  template<bool Enable = can_flatten<Source>>
  auto iterate()
  noexcept(noexcept(adapter(adapt::flatten(std::declval<Source>()))))
  -> decltype(adapter(adapt::flatten(std::declval<std::enable_if_t<Enable, Source>>()))) {
    return adapter(adapt::flatten(std::move(src_)));
  }

  /**
   * \brief Retrieve begin iterator.
   */
  constexpr auto begin() -> iterator {
    return iterator(src_);
  }

  /**
   * \brief Retrieve end iterator.
   */
  constexpr auto end() -> iterator {
    return iterator();
  }

  /**
   * \brief Apply functor on each value of the objpipe.
   *
   * \param[in] fn The functor to apply to each element.
   * \return \p fn
   */
  template<typename Fn>
  auto for_each(Fn&& fn) &&
  -> decltype(std::for_each(begin(), end(), std::forward<Fn>(fn))) {
    return std::for_each(begin(), end(), std::forward<Fn>(fn));
  }

  /**
   * \brief Perform a reduction.
   *
   * \note Accumulate uses a left-fold operation.
   * \param[in] fn A binary function.
   * \return The result of the accumulation, or an empty optional if the objpipe has no values.
   */
  template<typename Fn>
  constexpr auto accumulate(Fn&& fn) &&
  -> std::optional<value_type> {
    std::optional<value_type> result;
    objpipe_errc e = objpipe_errc::success;

    {
      transport<adapt::pull_type<Source>> v =
          adapt::raw_pull(src_);
      e = v.errc();

      if (e == objpipe_errc::success)
        result.emplace(std::move(v).value());
    }

    while (e == objpipe_errc::success) {
      transport<adapt::pull_type<Source>> v =
          adapt::raw_pull(src_);
      e = v.errc();

      if (e == objpipe_errc::success)
        result.emplace(std::invoke(fn, *std::move(result), std::move(v).value()));
    }

    if (e != objpipe_errc::success && e != objpipe_errc::closed) {
      throw std::system_error(
          static_cast<int>(e),
          objpipe_category());
    }
    return result;
  }

  /**
   * \brief Perform a reduction.
   *
   * \note Accumulate uses a left-fold operation.
   * \param[in] init The initial value of the accumulation.
   * \param[in] fn A binary function.
   * \return The result of the accumulation.
   */
  template<typename Init, typename Fn>
  constexpr auto accumulate(Init init, Fn&& fn) &&
  -> Init {
    objpipe_errc e = objpipe_errc::success;

    while (e == objpipe_errc::success) {
      transport<adapt::pull_type<Source>> v =
          adapt::raw_pull(src_);
      e = v.errc();

      if (e == objpipe_errc::success)
        init = std::invoke(fn, std::move(init), std::move(v).value());
    }

    if (e != objpipe_errc::success && e != objpipe_errc::closed) {
      throw std::system_error(
          static_cast<int>(e),
          objpipe_category());
    }
    return init;
  }

  /**
   * \brief Perform a reduction.
   *
   * \note The order in which the reduction is performed is unspecified.
   * \param[in] fn A binary function.
   * \return The result of the reduction, or an empty optional if the objpipe is empty.
   */
  template<typename Fn>
  constexpr auto reduce(Fn&& fn) &&
  -> std::optional<value_type> {
    return std::move(*this).accumulate(std::forward<Fn>(fn));
  }

  /**
   * \brief Perform a reduction.
   *
   * \note The order in which the reduction is performed is unspecified.
   * \param[in] init The initial value of the reduction.
   * \param[in] fn A binary function.
   * \return The result of the reduction.
   */
  template<typename Init, typename Fn>
  constexpr auto reduce(Init&& init, Fn&& fn) &&
  -> Init {
    return std::move(*this).accumulate(std::forward<Init>(init),
        std::forward<Fn>(fn));
  }

  /**
   * \brief Count the number of elements in the objpipe.
   * \return The number of elements in the objpipe.
   */
  constexpr auto count() &&
  -> std::uintmax_t {
    std::uintmax_t result = 0;
    objpipe_errc e = src_.pop_front();
    while (e == objpipe_errc::success) {
      ++result;
      e = src_.pop_front();
    }

    if (e != objpipe_errc::closed) {
      throw std::system_error(
          static_cast<int>(e),
          objpipe_category());
    }
    return result;
  }

  /**
   * \brief Copy the values in the objpipe into the given output iterator.
   *
   * \param[out] out An output iterator to which the values are to be copied.
   * \return \p out
   */
  template<typename OutputIterator>
  auto copy(OutputIterator&& out) &&
  -> decltype(std::copy(begin(), end(), std::forward<OutputIterator>(out))) {
    return std::copy(begin(), end(), std::forward<OutputIterator>(out));
  }

  /**
   * \brief Retrieve the min element from the objpipe.
   *
   * \param[in] pred A predicate to compare values.
   * \return The min value.
   * If the objpipe has no elements, an empty optional is returned.
   */
  template<typename Pred = std::less<value_type>>
  auto min(Pred&& pred = Pred()) &&
  -> std::optional<value_type> {
    std::optional<value_type> result;

    for (;;) {
      transport<adapt::pull_type<Source>> v = src_.pull();
      if (!v.has_value()) {
        assert(v.errc() != objpipe_errc::success);
        if (v.errc() == objpipe_errc::closed) break;
        throw std::system_error(
            static_cast<int>(v.errc()),
            objpipe_category());
      }

      if (!result.has_value()) {
        result.emplace(std::move(v).value());
      } else {
        const auto& result_value = *result;
        const auto& v_value = v.value();
        if (std::invoke(pred, v_value, result_value))
          result.emplace(std::move(v).value());
      }
    }
    return result;
  }

  /**
   * \brief Retrieve the max element from the objpipe.
   *
   * \param[in] pred A predicate to compare values.
   * \return The max value.
   * If the objpipe has no elements, an empty optional is returned.
   */
  template<typename Pred = std::less<value_type>>
  auto max(Pred&& pred = Pred()) &&
  -> std::optional<value_type> {
    std::optional<value_type> result;

    for (;;) {
      transport<adapt::pull_type<Source>> v = src_.pull();
      if (!v.has_value()) {
        assert(v.errc() != objpipe_errc::success);
        if (v.errc() == objpipe_errc::closed) break;
        throw std::system_error(
            static_cast<int>(v.errc()),
            objpipe_category());
      }

      if (!result.has_value()) {
        result.emplace(std::move(v).value());
      } else {
        const auto& result_value = *result;
        const auto& v_value = v.value();
        if (std::invoke(pred, result_value, v_value))
          result.emplace(std::move(v).value());
      }
    }
    return result;
  }

  ///\brief Retrieve objpipe values as a vector.
  template<typename Alloc = std::allocator<value_type>>
  auto to_vector(Alloc alloc = Alloc()) &&
  -> std::vector<value_type, Alloc> {
    return std::vector<value_type, Alloc>(begin(), end(), alloc);
  }

  ///\brief Convert adapter into a reader.
  operator reader<value_type>() && {
    return std::move(*this).as_reader();
  }

  ///\brief Convert adapter into a reader.
  auto as_reader() &&
  -> reader<value_type> {
    return reader<value_type>(virtual_pipe<value_type>(std::move(src_)));
  }

  /**
   * \brief Perform a sequence of operations on an objpipe.
   *
   * \details
   * This allows a pattern of
   * \code
   * MyObjPipe
   *     .perform(SequenceOfOperationsFn)
   * \endcode
   * to be used, in favour of
   * \code
   * SequenceOfOperations(MyObjPipe)
   * \endcode
   *
   * The latter becomes very hard to read when using chains:
   * \code
   * SequenceOfOperations(
   *     of(1, 2, 3, 4)
   *         .map(...)
   *         .map(...)
   *         .filter(...)
   *         // etc
   *     )
   *     .map(...)
   *     // etc
   *     ;
   * \endcode
   *
   * The nesting of the former becomes nightmarish, especially if multiple
   * functions are done thusly.
   *
   * Instead, replace with:
   * \code
   * of(1, 2, 3, 4)
   *     .map(...)
   *     .map(...)
   *     .filter(...)
   *     // etc
   *     .perform(SequenceOfOperations)
   *     .map(...)
   *     // etc
   *     ;
   * \endcode
   * This has the advantage that it shows SequenceOfOperations at the point in
   * the call chain where it is actually used.
   *
   * \param seq_op [in] An invokable.
   * It is invoked with this objpipe passed rvalue reference.
   *
   * \note \p seq_op is not required to return an objpipe.
   * It is valid to evaluate the objpipe.
   */
  template<typename SeqOp>
  auto perform(SeqOp&& seq_op) &&
  noexcept(is_nothrow_invocable_v<std::add_rvalue_reference_t<SeqOp>, adapter_t&&>)
  -> invoke_result_t<std::add_rvalue_reference_t<SeqOp>, adapter_t&&> {
    return std::invoke(std::forward<SeqOp>(seq_op), std::move(*this));
  }

  /**
   * \brief Retrieve the underlying source.
   */
  auto underlying() const &
  -> const Source& {
    return src_;
  }

  /**
   * \brief Retrieve the underlying source.
   */
  auto underlying() &
  -> Source& {
    return src_;
  }

  /**
   * \brief Retrieve the underlying source.
   */
  auto underlying() &&
  -> Source&& {
    return std::move(src_);
  }

 private:
  Source src_;
  front_store_handler<Source> store_;
};

namespace {
template<typename Source> void adapter_fn_(const adapter_t<Source>&) {}
template<typename T, typename = void>
struct is_adapter_
: std::false_type
{};
template<typename T>
struct is_adapter_<T, std::void_t<decltype(adapter_fn_(std::declval<T>()))>>
: std::true_type
{};
}
///\brief Tests if T is an adapter.
template<typename T>
using is_adapter = typename is_adapter_<std::decay_t<T>>::type;
///\brief Tests if T is an adapter.
template<typename T>
constexpr bool is_adapter_v = is_adapter<T>::value;

template<typename T, bool = is_adapter_v<T>>
struct adapter_underlying_type_ {};
template<typename T>
struct adapter_underlying_type_<T, true> {
  using type = std::decay_t<decltype(std::declval<T>().underlying())>;
};
template<typename T>
struct adapter_underlying_type
: adapter_underlying_type_<T>
{};
template<typename T>
using adapter_underlying_type_t = typename adapter_underlying_type<T>::type;

static_assert(is_adapter_v<reader<int>>,
    "is_adapter does not function properly for reader");
static_assert(std::is_same_v<adapter_underlying_type_t<reader<int>>, virtual_pipe<int>>,
    "adapter_underlying_type does not function properly for reader");


} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_ADAPTER_H */
