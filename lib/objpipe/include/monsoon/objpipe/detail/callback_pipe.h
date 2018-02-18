#ifndef MONSOON_OBJPIPE_DETAIL_CALLBACK_PIPE_H
#define MONSOON_OBJPIPE_DETAIL_CALLBACK_PIPE_H

///\file
///\ingroup objpipe_detail

#include <type_traits>
#include <utility>
#include <variant>
#include <boost/coroutine2/protected_fixedsize_stack.hpp>
#include <boost/coroutine2/coroutine.hpp>

namespace monsoon::objpipe::detail {


template<typename T>
class callback_push {
 private:
  using impl_type =
      typename boost::coroutines2::coroutine<std::add_pointer_t<T>>::push_type;

 public:
  constexpr callback_push(impl_type&& impl)
  : impl_(impl)
  {}

  void operator()(const T& v) {
    (*this)(T(v)); // Copy
  }

  void operator()(T&& v) {
    impl_(std::addressof(v));
  }

 private:
  impl_type impl_;
};

template<typename T>
class callback_push<const T> {
 private:
  using impl_type =
      typename boost::coroutines2::coroutine<std::add_pointer_t<T>>::push_type;

 public:
  constexpr callback_push(impl_type&& impl)
  : impl_(impl)
  {}

  void operator()(const T& v) {
    impl_(std::addressof(v));
  }

 private:
  impl_type impl_;
};

template<typename T, typename Fn>
class callback_fn_wrapper {
 public:
  constexpr callback_fn_wrapper(Fn&& fn)
  noexcept(std::is_nothrow_move_constructible_v<Fn>)
  : fn_(std::move(fn))
  {}

  constexpr callback_fn_wrapper(const Fn& fn)
  noexcept(std::is_nothrow_copy_constructible_v<Fn>)
  : fn_(fn)
  {}

  auto operator()(boost::coroutines2::coroutine<std::add_pointer_t<T>>&& push)
  -> void {
    std::invoke(
        std::move(fn_),
        callback_fn_wrapper<T>(std::forward<Push>(push)));
  }

 private:
  Fn fn_;
};

/**
 * \brief Extract values from fn, which emits them via a callback.
 * \ingroup objpipe_detail
 *
 * \details
 * This implementation creates a coroutine for the given functor.
 *
 * The functor is given an argument that implements two function operations:
 * \code
 * void operator()(T&&);
 * void operator()(const T&);
 * \endcode
 *
 * Except when the \p T argument is const, in which case only one function operation is implemented in the callback:
 * \code
 * void operator()(const T&);
 * \endcode
 *
 * The function is to emit values using the callback, which then transports
 * the value to the objpipe.
 *
 * The full setup of the coroutine is not done until the first time the objpipe
 * is read from.
 *
 * \note The implementation wraps the boost::coroutines2 push_type,
 * so the type passed to the function is not that.
 *
 * \tparam T The type of elements yielded by the function.
 * This should not be the plain type, without reference, const, or volatile qualifiers.
 * \tparam Fn The function type, which is to yield values using a callback.
 */
template<typename T, typename Fn>
class callback_pipe {
  static_assert(!std::is_volatile_v<T>,
      "Type argument to callback may not be volatile.");
  static_assert(!std::is_lvalue_reference_v<T>
      && !std::is_rvalue_reference_v<T>,
      "Type argument to callback may not be a reference.");

 private:
  using coro_t = boost::coroutines2::coroutine<std::add_pointer_t<T>>;
  using fn_type = callback_fn_wrapper<Fn>;
  using result_type = std::conditional_t<std::is_const_v<T>,
        std::add_lvalue_reference_t<T>,
        std::add_rvalue_reference_t<T>>;

 public:
  constexpr callback_pipe(Fn&& fn)
  noexcept(std::is_nothrow_move_constructible_v<Fn>)
  : src_(std::in_place_index<0>, std::move(fn))
  {}

  constexpr callback_pipe(const Fn& fn)
  noexcept(std::is_nothrow_copy_constructible_v<Fn>)
  : src_(std::in_place_index<0>, fn)
  {}

  auto is_pullable() const
  noexcept
  -> bool {
    ensure_init_();
    return bool(std::get<1>(src_));
  }

  auto wait() const
  -> objpipe_errc {
    ensure_init_();
    return (std::get<1>(src_) ? objpipe_errc::success : objpipe_errc::closed);
  }

  auto front() const
  -> std::variant<result_type, objpipe_errc> {
    ensure_init_();
    auto& coro = std::get<1>(src_);

    if (!coro) return { std::in_place_index<1>, objpipe_errc::closed };
    return { std::in_place_index<0>, get() };
  }

  auto pop_front()
  -> objpipe_errc {
    ensure_init_();
    auto& coro = std::get<1>(src_);

    if (!coro) return objpipe_errc::closed;
    coro();
    return objpipe_errc::success;
  }

  auto pull() const
  -> std::variant<result_type, objpipe_errc> {
    ensure_init_();
    auto& coro = std::get<1>(src_);

    if (!coro) return { std::in_place_index<1>, objpipe_errc::closed };
    must_advance_ = true;
    return { std::in_place_index<0>, get() };
  }

  auto try_pull() const
  -> std::variant<result_type, objpipe_errc> {
    ensure_init_();
    auto& coro = std::get<1>(src_);

    if (!coro) return { std::in_place_index<1>, objpipe_errc::closed };
    must_advance_ = true;
    return { std::in_place_index<0>, get() };
  }

 private:
  void ensure_init_() const {
    if (src_.index() == 0) {
      assert(!must_advance_);
      Fn fn = std::get<0>(std::move(src_));
      src_.emplace(std::in_place_index<1>,
          boost::coroutines2::protected_fixedsize_stack(),
          std::move(fn));
    }

    if (std::exchange(must_advance_)) std::get<1>(src_)();
  }

  auto get() const
  -> result_type {
    assert(src_);
    if constexpr(std::is_const_v<result_type>)
      return *src_.get();
    else
      return std::move(*src_.get());
  }

  mutable std::variant<fn_type, coro_t::pull_type> src_;
  mutable bool must_advance_ = false;
};


} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_CALLBACK_PIPE_H */
