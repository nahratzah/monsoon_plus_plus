#ifndef MONSOON_OBJPIPE_DETAIL_FUNCTOR_H
#define MONSOON_OBJPIPE_DETAIL_FUNCTOR_H

///\file
///\ingroup objpipe_detail

#include <type_traits>
#include <functional>
#include <memory>
#include <monsoon/objpipe/detail/invocable_.h>

namespace monsoon::objpipe::detail {


///\brief Default case of a functor: move constructible and swappable.
template<typename Fn>
class dfl_functor {
  static_assert(std::is_move_constructible_v<Fn>,
      "This functor requires Fn to be move constructible.");
  static_assert(std::is_swappable_v<Fn>,
      "This functor requires Fn to be move constructible.");

 public:
  dfl_functor(Fn&& fn)
  noexcept(std::is_nothrow_move_constructible_v<Fn>)
  : fn_(std::move(fn))
  {}

  dfl_functor(const Fn& fn)
  noexcept(std::is_nothrow_copy_constructible_v<Fn>)
  : fn_(std::move(fn))
  {}

  friend auto swap(dfl_functor& x, dfl_functor& y)
  noexcept(std::is_nothrow_swappable_v<Fn>)
  -> void {
    using std::swap;
    swap(*x, *y);
  }

  template<typename... Args>
  auto operator()(Args&&... args) const
  noexcept(is_nothrow_invocable_v<const Fn&, Args...>)
  -> invoke_result_t<const Fn&, Args...> {
    return std::invoke(**this, std::forward<Args>(args)...);
  }

  template<typename... Args>
  auto operator()(Args&&... args)
  noexcept(is_nothrow_invocable_v<Fn&, Args...>)
  -> invoke_result_t<Fn&, Args...> {
    return std::invoke(**this, std::forward<Args>(args)...);
  }

  auto operator*() const noexcept
  -> const Fn& {
    return fn_;
  }

  auto operator*() noexcept
  -> Fn& {
    return fn_;
  }

  auto operator->() const noexcept
  -> const Fn* {
    return std::addressof(**this);
  }

  auto operator->() noexcept
  -> Fn* {
    return std::addressof(**this);
  }

 private:
  Fn fn_;
};

///\brief Empty-member-optimization of a functor.
template<typename Fn>
class emo_functor
: public Fn
{
  static_assert(std::is_move_constructible_v<Fn>,
      "This functor requires Fn to be move constructible.");
  static_assert(std::is_swappable_v<Fn>,
      "This functor requires Fn to be move constructible.");
  static_assert(!std::is_polymorphic_v<Fn>,
      "This functor should not be used for polymorphic functors.");
  static_assert(!std::is_final_v<Fn>,
      "This functor does not support final functors.");

 public:
  emo_functor(Fn&& fn)
  noexcept(std::is_nothrow_move_constructible_v<Fn>)
  : Fn(std::move(fn))
  {}

  emo_functor(const Fn& fn)
  noexcept(std::is_nothrow_copy_constructible_v<Fn>)
  : Fn(fn)
  {}

  friend auto swap(emo_functor& x, emo_functor& y)
  noexcept(std::is_nothrow_swappable_v<Fn>)
  -> void {
    using std::swap;
    swap(*x, *y);
  }

  template<typename... Args>
  auto operator()(Args&&... args) const
  noexcept(is_nothrow_invocable_v<const Fn&, Args...>)
  -> invoke_result_t<const Fn&, Args...> {
    return std::invoke(**this, std::forward<Args>(args)...);
  }

  template<typename... Args>
  auto operator()(Args&&... args)
  noexcept(is_nothrow_invocable_v<Fn&, Args...>)
  -> invoke_result_t<Fn&, Args...> {
    return std::invoke(**this, std::forward<Args>(args)...);
  }

  auto operator*() const noexcept
  -> const Fn& {
    return *this;
  }

  auto operator*() noexcept
  -> Fn& {
    return *this;
  }

  auto operator->() const noexcept
  -> const Fn* {
    return std::addressof(**this);
  }

  auto operator->() noexcept
  -> Fn* {
    return std::addressof(**this);
  }
};

///\brief Functor that is swappable, by being implemented in terms of move semantics.
///\details Internally, placement new is used to construct the functor during swap.
template<typename Fn>
class lambda_functor_with_nothrow_move {
  static_assert(std::is_move_constructible_v<Fn>,
      "This functor requires Fn to be move constructible.");
  static_assert(!std::is_swappable_v<Fn>,
      "This class is slightly complicated and should only be used to adapt non-swappable functors, such as lambdas.");
  static_assert(std::is_nothrow_move_constructible_v<Fn>,
      "This functor requires Fn to be noexcept move constructible.");
  static_assert(std::is_nothrow_destructible_v<Fn>,
      "This functor requires Fn to be noexcept destructible.");

 private:
  using storage = std::aligned_storage_t<sizeof(Fn), std::alignment_of_v<Fn>>;

 public:
  lambda_functor_with_nothrow_move() = delete;

  lambda_functor_with_nothrow_move(lambda_functor_with_nothrow_move&& other)
  noexcept
  : lambda_functor_with_nothrow_move(std::move(*other))
  {}

  lambda_functor_with_nothrow_move(const lambda_functor_with_nothrow_move& other)
  noexcept
  : lambda_functor_with_nothrow_move(*other)
  {}

  // In order to make this move assignible.
  auto operator=(lambda_functor_with_nothrow_move&& other)
  noexcept
  -> lambda_functor_with_nothrow_move& {
    (*this)->~Fn();
    new (&store_) Fn(std::move(*other));
    return *this;
  }

  // Delete copy assignment.
  // We could conditionally erase it, only allowing it to exist
  // if copy assignment is noexcept.
  // But there is no use in that, as this is internally used only for
  // functors that are moved.
  auto operator=(const lambda_functor_with_nothrow_move& other) -> lambda_functor_with_nothrow_move& = delete;

  lambda_functor_with_nothrow_move(Fn&& fn)
  noexcept
  {
    new (&store_) Fn(std::move(fn));
  }

  lambda_functor_with_nothrow_move(const Fn& fn)
  noexcept(std::is_nothrow_copy_constructible_v<Fn>)
  {
    new (&store_) Fn(fn);
  }

  ~lambda_functor_with_nothrow_move()
  noexcept
  {
    (*this)->~Fn();
  }

  template<typename... Args>
  auto operator()(Args&&... args) const
  noexcept(is_nothrow_invocable_v<const Fn&, Args...>)
  -> invoke_result_t<const Fn&, Args...> {
    return std::invoke(**this, std::forward<Args>(args)...);
  }

  template<typename... Args>
  auto operator()(Args&&... args)
  noexcept(is_nothrow_invocable_v<Fn&, Args...>)
  -> invoke_result_t<Fn&, Args...> {
    return std::invoke(**this, std::forward<Args>(args)...);
  }

  auto operator*() const noexcept
  -> const Fn& {
    return *reinterpret_cast<const Fn*>(&store_);
  }

  auto operator*() noexcept
  -> Fn& {
    return *reinterpret_cast<Fn*>(&store_);
  }

  auto operator->() const noexcept
  -> const Fn* {
    return std::addressof(**this);
  }

  auto operator->() noexcept
  -> Fn* {
    return std::addressof(**this);
  }

 private:
  storage store_;
};

/**
 * \brief Using an optional as storage for Fn.
 * \details
 * We use an optional, which keeps track of wether the value is or is not assigned.
 * Small payment in the overhead of optional state tracking, but that's unavoidable
 * unless we're willing to abort() in the case of exceptions
 * (like lambda_functor_with_nothrow_move has to do).
 */
template<typename Fn>
class lambda_functor_with_throwing_move {
  static_assert(std::is_move_constructible_v<Fn>,
      "This functor requires Fn to be move constructible.");
  static_assert(!std::is_swappable_v<Fn>,
      "This class has storage overhead and should only be used to adapt non-swappable functors, such as lambdas.");

 public:
  lambda_functor_with_throwing_move(Fn&& fn)
  : fn_(std::move(fn))
  {}

  lambda_functor_with_throwing_move(const Fn& fn)
  : fn_(fn)
  {}

  friend auto swap(lambda_functor_with_throwing_move& x, lambda_functor_with_throwing_move& y)
  noexcept(std::is_nothrow_move_constructible_v<Fn> && std::is_nothrow_destructible_v<Fn>)
  -> void {
    std::optional<Fn> tmp = std::move(*x);
    x.fn_.emplace(std::move(*y));
    y.fn_.emplace(*std::move(tmp));
  }

  template<typename... Args>
  auto operator()(Args&&... args) const
  noexcept(is_nothrow_invocable_v<const Fn&, Args...>)
  -> invoke_result_t<const Fn&, Args...> {
    return std::invoke(**this, std::forward<Args>(args)...);
  }

  template<typename... Args>
  auto operator()(Args&&... args)
  noexcept(is_nothrow_invocable_v<Fn&, Args...>)
  -> invoke_result_t<Fn&, Args...> {
    return std::invoke(**this, std::forward<Args>(args)...);
  }

  auto operator*() const noexcept
  -> const Fn& {
    assert(fn_.has_value());
    return *fn_;
  }

  auto operator*() noexcept
  -> Fn& {
    assert(fn_.has_value());
    return *fn_;
  }

  auto operator->() const noexcept
  -> const Fn* {
    return std::addressof(**this);
  }

  auto operator->() noexcept
  -> Fn* {
    return std::addressof(**this);
  }

 private:
  std::optional<Fn> fn_;
};

///\brief Implementation of the functor selector.
///\details
/// Selector implementation, since std::conditional requires the type to be
/// instantiable, which we prohibit due to static assertions (as well as
/// deriving in the case of emo_functor).
template<typename Fn,
    bool Swappable = std::is_swappable_v<Fn>,
    bool NothrowMoveDestroy = std::is_nothrow_move_constructible_v<Fn> && std::is_nothrow_destructible_v<Fn>,
    bool EmptyMemberOptimization = !std::is_polymorphic_v<Fn> && !std::is_final_v<Fn> && std::is_empty_v<Fn>
    >
struct functor_;

template<typename Fn, bool NothrowMoveDestroy>
struct functor_<Fn, true, NothrowMoveDestroy, false> {
  using type = dfl_functor<Fn>;
};

template<typename Fn, bool NothrowMoveDestroy>
struct functor_<Fn, true, NothrowMoveDestroy, true> {
  using type = emo_functor<Fn>;
};

template<typename Fn, bool EmptyMemberOptimization>
struct functor_<Fn, false, true, EmptyMemberOptimization> {
  using type = lambda_functor_with_nothrow_move<Fn>;
};

template<typename Fn, bool EmptyMemberOptimization>
struct functor_<Fn, false, false, EmptyMemberOptimization> {
  using type = lambda_functor_with_throwing_move<Fn>;
};

///\brief Select a functor wrapper type for \p Fn.
///\tparam Fn A functor type.
///\details
///The functor type shall be move constructible, swappable, and invocable.
///\note Lambdas are not swappable, hence the need for a wrapper type.
///
///Functors are also dereferencable.
///And the operator() for each is templated, so the type can be used for
///non-functors too.
///\bug Misnamed as functor, should be called something more descriptive of
///why it exists: to allow move-only types to be swappable.
template<typename Fn>
using functor = typename functor_<Fn>::type;


} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_FUNCTOR_H */
