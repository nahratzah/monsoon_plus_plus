#ifndef MONSOON_OBJPIPE_DETAIL_FUNCTOR_H
#define MONSOON_OBJPIPE_DETAIL_FUNCTOR_H

///\file
///\brief Utility type to make move-constructible types swappable.
///\ingroup objpipe_detail

#include <type_traits>
#include <functional>
#include <memory>
#include <monsoon/objpipe/detail/invocable_.h>

namespace monsoon::objpipe::detail {


///\brief Default case of a swappable: move constructible and swappable.
template<typename T>
class dfl_swappable {
  static_assert(std::is_move_constructible_v<T>,
      "This swappable requires T to be move constructible.");
  static_assert(std::is_swappable_v<T>,
      "This swappable requires T to be move constructible.");

 public:
  dfl_swappable(T&& v)
  noexcept(std::is_nothrow_move_constructible_v<T>)
  : v_(std::move(v))
  {}

  dfl_swappable(const T& v)
  noexcept(std::is_nothrow_copy_constructible_v<T>)
  : v_(std::move(v))
  {}

  friend auto swap(dfl_swappable& x, dfl_swappable& y)
  noexcept(std::is_nothrow_swappable_v<T>)
  -> void {
    using std::swap;
    swap(*x, *y);
  }

  template<typename... Args>
  auto operator()(Args&&... args) const
  noexcept(is_nothrow_invocable_v<const T&, Args...>)
  -> invoke_result_t<const T&, Args...> {
    return std::invoke(**this, std::forward<Args>(args)...);
  }

  template<typename... Args>
  auto operator()(Args&&... args)
  noexcept(is_nothrow_invocable_v<T&, Args...>)
  -> invoke_result_t<T&, Args...> {
    return std::invoke(**this, std::forward<Args>(args)...);
  }

  auto operator*() const noexcept
  -> const T& {
    return v_;
  }

  auto operator*() noexcept
  -> T& {
    return v_;
  }

  auto operator->() const noexcept
  -> const T* {
    return std::addressof(**this);
  }

  auto operator->() noexcept
  -> T* {
    return std::addressof(**this);
  }

 private:
  T v_;
};

///\brief Empty-member-optimization of a swappable.
template<typename T>
class emo_swappable
: public T
{
  static_assert(std::is_move_constructible_v<T>,
      "This swappable requires T to be move constructible.");
  static_assert(std::is_swappable_v<T>,
      "This swappable requires T to be move constructible.");
  static_assert(!std::is_polymorphic_v<T>,
      "This swappable should not be used for polymorphic types.");
  static_assert(!std::is_final_v<T>,
      "This swappable does not support final types.");

 public:
  emo_swappable(T&& v)
  noexcept(std::is_nothrow_move_constructible_v<T>)
  : T(std::move(v))
  {}

  emo_swappable(const T& v)
  noexcept(std::is_nothrow_copy_constructible_v<T>)
  : T(v)
  {}

  friend auto swap(emo_swappable& x, emo_swappable& y)
  noexcept(std::is_nothrow_swappable_v<T>)
  -> void {
    using std::swap;
    swap(*x, *y);
  }

  template<typename... Args>
  auto operator()(Args&&... args) const
  noexcept(is_nothrow_invocable_v<const T&, Args...>)
  -> invoke_result_t<const T&, Args...> {
    return std::invoke(**this, std::forward<Args>(args)...);
  }

  template<typename... Args>
  auto operator()(Args&&... args)
  noexcept(is_nothrow_invocable_v<T&, Args...>)
  -> invoke_result_t<T&, Args...> {
    return std::invoke(**this, std::forward<Args>(args)...);
  }

  auto operator*() const noexcept
  -> const T& {
    return *this;
  }

  auto operator*() noexcept
  -> T& {
    return *this;
  }

  auto operator->() const noexcept
  -> const T* {
    return std::addressof(**this);
  }

  auto operator->() noexcept
  -> T* {
    return std::addressof(**this);
  }
};

///\brief Functor that is swappable, by being implemented in terms of move semantics.
///\details Internally, placement new is used to construct the type during swap.
template<typename T>
class lambda_swappable_with_nothrow_move {
  static_assert(std::is_move_constructible_v<T>,
      "This swappable requires T to be move constructible.");
  static_assert(!std::is_swappable_v<T>,
      "This class is slightly complicated and should only be used to adapt non-swappable types, such as lambdas.");
  static_assert(std::is_nothrow_move_constructible_v<T>,
      "This swappable requires T to be noexcept move constructible.");
  static_assert(std::is_nothrow_destructible_v<T>,
      "This swappable requires T to be noexcept destructible.");

 private:
  using storage = std::aligned_storage_t<sizeof(T), std::alignment_of_v<T>>;

 public:
  lambda_swappable_with_nothrow_move() = delete;

  lambda_swappable_with_nothrow_move(lambda_swappable_with_nothrow_move&& other)
  noexcept
  : lambda_swappable_with_nothrow_move(std::move(*other))
  {}

  lambda_swappable_with_nothrow_move(const lambda_swappable_with_nothrow_move& other)
  noexcept
  : lambda_swappable_with_nothrow_move(*other)
  {}

  // In order to make this move assignible.
  auto operator=(lambda_swappable_with_nothrow_move&& other)
  noexcept
  -> lambda_swappable_with_nothrow_move& {
    (*this)->~T();
    new (&store_) T(std::move(*other));
    return *this;
  }

  auto operator=(const lambda_swappable_with_nothrow_move& other)
  noexcept(std::is_nothrow_copy_constructible_v<T>)
  -> lambda_swappable_with_nothrow_move& {
    // Copy and swap.
    swap(*this, lambda_swappable_with_nothrow_move(other));
    return *this;
  }

  lambda_swappable_with_nothrow_move(T&& v)
  noexcept
  {
    new (&store_) T(std::move(v));
  }

  lambda_swappable_with_nothrow_move(const T& v)
  noexcept(std::is_nothrow_copy_constructible_v<T>)
  {
    new (&store_) T(v);
  }

  ~lambda_swappable_with_nothrow_move()
  noexcept
  {
    (*this)->~T();
  }

  template<typename... Args>
  auto operator()(Args&&... args) const
  noexcept(is_nothrow_invocable_v<const T&, Args...>)
  -> invoke_result_t<const T&, Args...> {
    return std::invoke(**this, std::forward<Args>(args)...);
  }

  template<typename... Args>
  auto operator()(Args&&... args)
  noexcept(is_nothrow_invocable_v<T&, Args...>)
  -> invoke_result_t<T&, Args...> {
    return std::invoke(**this, std::forward<Args>(args)...);
  }

  auto operator*() const noexcept
  -> const T& {
    return *reinterpret_cast<const T*>(&store_);
  }

  auto operator*() noexcept
  -> T& {
    return *reinterpret_cast<T*>(&store_);
  }

  auto operator->() const noexcept
  -> const T* {
    return std::addressof(**this);
  }

  auto operator->() noexcept
  -> T* {
    return std::addressof(**this);
  }

 private:
  storage store_;
};

/**
 * \brief Using an optional as storage for T.
 * \details
 * We use an optional, which keeps track of wether the value is or is not assigned.
 * Small payment in the overhead of optional state tracking, but that's unavoidable
 * unless we're willing to abort() in the case of exceptions
 * (like lambda_swappable_with_nothrow_move has to do).
 */
template<typename T>
class lambda_swappable_with_throwing_move {
  static_assert(std::is_move_constructible_v<T>,
      "This swappable requires T to be move constructible.");
  static_assert(!std::is_swappable_v<T>,
      "This class has storage overhead and should only be used to adapt non-swappable types that throw during move, such as lambdas.");

 public:
  lambda_swappable_with_throwing_move(T&& v)
  : v_(std::move(v))
  {}

  lambda_swappable_with_throwing_move(const T& v)
  : v_(v)
  {}

  friend auto swap(lambda_swappable_with_throwing_move& x, lambda_swappable_with_throwing_move& y)
  noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_destructible_v<T>)
  -> void {
    std::optional<T> tmp = std::move(*x);
    x.v_.emplace(std::move(*y));
    y.v_.emplace(*std::move(tmp));
  }

  template<typename... Args>
  auto operator()(Args&&... args) const
  noexcept(is_nothrow_invocable_v<const T&, Args...>)
  -> invoke_result_t<const T&, Args...> {
    return std::invoke(**this, std::forward<Args>(args)...);
  }

  template<typename... Args>
  auto operator()(Args&&... args)
  noexcept(is_nothrow_invocable_v<T&, Args...>)
  -> invoke_result_t<T&, Args...> {
    return std::invoke(**this, std::forward<Args>(args)...);
  }

  auto operator*() const noexcept
  -> const T& {
    assert(v_.has_value());
    return *v_;
  }

  auto operator*() noexcept
  -> T& {
    assert(v_.has_value());
    return *v_;
  }

  auto operator->() const noexcept
  -> const T* {
    return std::addressof(**this);
  }

  auto operator->() noexcept
  -> T* {
    return std::addressof(**this);
  }

 private:
  std::optional<T> v_;
};

///\brief Implementation of the swappable selector.
///\details
/// Selector implementation, since std::conditional requires the type to be
/// instantiable, which we prohibit due to static assertions (as well as
/// deriving in the case of emo_swappable).
template<typename T,
    bool Swappable = std::is_swappable_v<T>,
    bool NothrowMoveDestroy = std::is_nothrow_move_constructible_v<T> && std::is_nothrow_destructible_v<T>,
    bool EmptyMemberOptimization = !std::is_polymorphic_v<T> && !std::is_final_v<T> && std::is_empty_v<T>
    >
struct swappable_;

template<typename T, bool NothrowMoveDestroy>
struct swappable_<T, true, NothrowMoveDestroy, false> {
  using type = dfl_swappable<T>;
};

template<typename T, bool NothrowMoveDestroy>
struct swappable_<T, true, NothrowMoveDestroy, true> {
  using type = emo_swappable<T>;
};

template<typename T, bool EmptyMemberOptimization>
struct swappable_<T, false, true, EmptyMemberOptimization> {
  using type = lambda_swappable_with_nothrow_move<T>;
};

template<typename T, bool EmptyMemberOptimization>
struct swappable_<T, false, false, EmptyMemberOptimization> {
  using type = lambda_swappable_with_throwing_move<T>;
};

///\brief Select a swappable wrapper type for \p T.
///\tparam T A type that is move constructible and may or may not be swappable.
///\details
///The swappable is move constructible, swappable, and invocable.
///\note Lambdas are not swappable, hence the need for a wrapper type.
///
///Swappables are also dereferencable.
///And the operator() for each is templated, so the type can be used for
///non-functors too.
template<typename T>
using swappable = typename swappable_<T>::type;


} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_FUNCTOR_H */
