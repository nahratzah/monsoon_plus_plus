#ifndef MONSOON_OBJPIPE_DETAIL_TRANSFORM_OP_H
#define MONSOON_OBJPIPE_DETAIL_TRANSFORM_OP_H

///\file
///\ingroup objpipe_detail

#include <functional>
#include <type_traits>
#include <utility>
#include <variant>
#include <monsoon/objpipe/detail/fwd.h>
#include <monsoon/objpipe/detail/adapt.h>
#include <monsoon/objpipe/detail/invocable_.h>

namespace monsoon::objpipe::detail {


struct transform_identity_fn {
  template<typename Arg>
  constexpr auto operator()(Arg&& arg) const
  -> std::add_rvalue_reference_t<Arg> {
    return std::move(arg);
  }
};

template<typename Arg, typename Fn, typename... Tail>
class transform_fn_adapter {
 private:
  using inner_fn_type = transform_fn_adapter<Arg, Fn>;
  using outer_fn_type = transform_fn_adapter<typename inner_fn_type::result_type, Tail...>;

 public:
  using result_type = typename outer_fn_type::result_type;

  template<typename Arg0, typename... Args>
  explicit constexpr transform_fn_adapter(Arg0&& arg0, Args&&... args)
  noexcept(std::is_nothrow_constructible_v<inner_fn_type, std::add_rvalue_reference_t<Arg0>>
      && std::is_nothrow_constructible_v<outer_fn_type, std::add_rvalue_reference_t<Args>...>)
  : inner_(std::forward<Arg0>(arg0)),
    outer_(std::forward<Args>(args)...)
  {}

  template<typename X>
  constexpr auto operator()(X&& x) const
  noexcept(is_nothrow_invocable_v<const inner_fn_type&, std::add_rvalue_reference_t<X>, const outer_fn_type&>)
  -> invoke_result_t<const inner_fn_type&, std::add_rvalue_reference_t<X>, const outer_fn_type&> {
    return std::invoke(inner_, std::forward<X>(x), outer_);
  }

  template<typename NextFn>
  constexpr auto extend(NextFn&& next_fn) &&
  noexcept(std::conjunction_v<
      std::is_nothrow_move_constructible<Fn>,
      std::is_nothrow_move_constructible<Tail>...,
      std::is_nothrow_constructible<std::decay_t<NextFn>, std::add_rvalue_reference_t<NextFn>>>)
  -> decltype(auto) {
    return transform_fn_adapter<Fn, Tail..., std::decay_t<NextFn>>(
        std::move(inner_),
        std::move(outer_).extend(std::forward<NextFn>(next_fn)));
  }

 private:
  inner_fn_type inner_;
  outer_fn_type outer_;
};

/**
 * \brief Propagate references for another type, only if T is a non-volatile reference.
 * \ingroup objpipe_detail
 *
 * \details
 * This helper type takes a template argument T,
 * including its const, volatile, and reference modifiers.
 *
 * It then uses those to decide if a related type is allowed to have its
 * modifiers preserved across function return.
 *
 * \tparam T The input type for a function.
 */
template<typename T>
struct propagate_copy {
  static constexpr bool is_volatile = std::is_volatile_v<T>;
  static constexpr bool is_lref = std::is_lvalue_reference_v<T>;
  static constexpr bool is_rref = std::is_rvalue_reference_v<T>;
  ///\brief Decide if modifiers are to be propagated.
  static constexpr bool propagate = !is_volatile && (is_lref || is_rref);

  /**
   * \brief Propagate reference only if the input type is a reference.
   *
   * \details
   * Clears reference decorations from U, if those are not to be propagated.
   * Otherwise, U is passed through unchanged.
   *
   * \tparam U A type, including its const, volatile, and reference modifiers,
   * which are to be cleared iff they are not to be propagated.
   */
  template<typename U>
  using type = std::conditional_t<
      propagate,
      U,
      std::remove_cv_t<std::remove_reference_t<U>>>;
};

template<typename Arg, typename Fn>
class transform_fn_adapter<Arg, Fn> {
 private:
  using argument_type = Arg;
  using functor_type = Fn;

  static_assert(!std::is_const_v<functor_type>,
      "Functor may not be a const type.");
  static_assert(!std::is_volatile_v<functor_type>,
      "Functor may not be a volatile type.");
  static_assert(!std::is_lvalue_reference_v<functor_type>
      && !std::is_rvalue_reference_v<functor_type>,
      "Functor may not be a reference.");

  static_assert(!std::is_const_v<argument_type>,
      "Argument may not be a const type.");
  static_assert(!std::is_volatile_v<argument_type>,
      "Argument may not be a volatile type.");
  static_assert(!std::is_lvalue_reference_v<argument_type>
      && !std::is_rvalue_reference_v<argument_type>,
      "Argument may not be a reference.");

  using arg_cref = std::add_lvalue_reference_t<std::add_const_t<argument_type>>;
  using arg_lref = std::add_lvalue_reference_t<argument_type>;
  using arg_rref = std::add_rvalue_reference_t<argument_type>;

  static constexpr bool is_cref_invocable =
      is_invocable_v<const functor_type&, arg_cref>;
  static constexpr bool is_lref_invocable =
      is_invocable_v<const functor_type&, arg_lref>;
  static constexpr bool is_rref_invocable =
      is_invocable_v<const functor_type&, arg_rref>;

  static_assert(is_cref_invocable || is_lref_invocable || is_rref_invocable,
      "Functor must be invocable with the argument type");

  ///\brief Helper that decides if the returned value from an invocation chain is to be copied into the return value.
  template<typename NextFn, typename DecoratedArg>
  using propagate_type = typename propagate_copy<invoke_result_t<const functor_type&, DecoratedArg>>::template type<invoke_result_t<NextFn, invoke_result_t<const functor_type&, DecoratedArg>>>;

 public:
  ///\brief Plain result type of this functor adapter.
  using result_type = std::remove_reference_t<std::remove_cv_t<
      invoke_result_t<transform_fn_adapter, const argument_type&>>>;

  explicit constexpr transform_fn_adapter(const functor_type& fn)
  noexcept(std::is_nothrow_copy_constructible_v<functor_type>)
  : fn_(fn)
  {}

  explicit constexpr transform_fn_adapter(functor_type&& fn)
  noexcept(std::is_nothrow_move_constructible_v<functor_type>)
  : fn_(std::move(fn))
  {}

  template<bool Enable = is_cref_invocable, typename NextFn>
  constexpr auto operator()(arg_cref v,
      NextFn&& next_fn = transform_identity_fn()) const
  noexcept(is_nothrow_invocable_v<const functor_type&, arg_cref>)
  -> typename std::enable_if_t<Enable,
      propagate_type<NextFn, arg_cref>> {
    return std::invoke(next_fn, std::invoke(fn_, v));
  }

  template<bool Enable = !is_cref_invocable && is_rref_invocable, typename NextFn>
  constexpr auto operator()(arg_cref v,
      NextFn&& next_fn = transform_identity_fn()) const
  noexcept(is_nothrow_invocable_v<const functor_type&, arg_rref>
      && std::is_nothrow_copy_constructible_v<argument_type>)
  -> typename std::enable_if_t<Enable,
      std::remove_cv_t<std::remove_reference_t<invoke_result_t<NextFn, invoke_result_t<const functor_type&, arg_rref>>>>> {
    argument_type tmp = v;
    return std::invoke(next_fn, std::invoke(fn_, std::move(tmp)));
  }

  template<bool Enable = !is_cref_invocable && !is_rref_invocable && is_lref_invocable, typename NextFn>
  constexpr auto operator()(arg_cref v,
      NextFn&& next_fn = transform_identity_fn()) const
  noexcept(is_nothrow_invocable_v<const functor_type&, arg_lref>
      && std::is_nothrow_copy_constructible_v<argument_type>)
  -> typename std::enable_if_t<Enable,
      std::remove_cv_t<std::remove_reference_t<invoke_result_t<NextFn, invoke_result_t<const functor_type&, arg_lref>>>>> {
    argument_type tmp = v;
    return std::invoke(next_fn, std::invoke(fn_, tmp));
  }

  template<bool Enable = is_lref_invocable, typename NextFn>
  constexpr auto operator()(arg_lref v,
      NextFn&& next_fn = transform_identity_fn()) const
  noexcept(is_nothrow_invocable_v<const functor_type&, arg_lref>)
  -> typename std::enable_if_t<Enable,
      propagate_type<NextFn, arg_lref>> {
    return std::invoke(next_fn, std::invoke(fn_, v));
  }

  template<bool Enable = is_rref_invocable, typename NextFn>
  constexpr auto operator()(arg_rref v,
      NextFn&& next_fn = transform_identity_fn()) const
  noexcept(is_nothrow_invocable_v<const functor_type&, arg_rref>)
  -> typename std::enable_if_t<Enable,
      propagate_type<NextFn, arg_rref>> {
    return std::invoke(next_fn, std::invoke(fn_, std::move(v)));
  }

 private:
  functor_type fn_;
};

/**
 * \brief Implements the transform operation.
 * \ingroup objpipe_detail
 *
 * \tparam Source The source on which to operate.
 * \tparam Fn Functors to be invoked on the values.
 * Inner most function is mentioned first, outer most function is mentioned last.
 *
 * \sa \ref monsoon::objpipe::detail::adapter_t::transform
 */
template<typename Source, typename... Fn>
class transform_op {
 private:
  using raw_src_front_type = std::variant_alternative_t<
      0,
      decltype(std::declval<const Source&>().front())>;
  using fn_type =
      transform_fn_adapter<Fn..., std::remove_cv_t<std::remove_reference_t<raw_src_front_type>>>;
  using raw_front_type = decltype(std::invoke(std::declval<const fn_type&>(), std::declval<raw_src_front_type>()));
  using front_type = typename propagate_copy<raw_src_front_type>::template type<raw_front_type>;

  using raw_src_try_pull_type = std::variant_alternative_t<
      0,
      decltype(adapt::raw_try_pull(std::declval<Source&>()))>;
  using raw_try_pull_type = decltype(std::invoke(std::declval<const fn_type&>(), std::declval<raw_src_try_pull_type>()));
  using try_pull_type = typename propagate_copy<raw_src_try_pull_type>::template type<raw_try_pull_type>;

  using raw_src_pull_type = std::variant_alternative_t<
      0,
      decltype(adapt::raw_pull(std::declval<Source&>()))>;
  using raw_pull_type = decltype(std::invoke(std::declval<const fn_type&>(), std::declval<raw_src_pull_type>()));
  using pull_type = typename propagate_copy<raw_src_pull_type>::template type<raw_pull_type>;

  static_assert(std::is_same_v<std::decay_t<raw_src_front_type>, std::decay_t<raw_src_try_pull_type>>
      && std::is_same_v<std::decay_t<raw_src_front_type>, std::decay_t<raw_src_pull_type>>,
      "Decayed return types of front(), try_pull() and pull() in source must be the same");

 public:
  template<typename... FnAdapterArgs>
  explicit constexpr transform_op(Source&& src, FnAdapterArgs&&... fn_adapter_args)
  noexcept(std::is_nothrow_move_constructible_v<Source>
      && std::is_nothrow_constructible_v<fn_type, FnAdapterArgs&&...>)
  : src_(std::move(src)),
    fn_(std::forward(fn_adapter_args)...)
  {}

  constexpr auto is_pullable() const
  noexcept
  -> bool {
    return src_.is_pullable();
  }

  constexpr auto wait() const
  noexcept(noexcept(src_.wait()))
  -> objpipe_errc {
    return src_.wait();
  }

  constexpr auto front() const
  noexcept(noexcept(invoke_fn_(src_.front()))
      && (std::is_lvalue_reference_v<front_type>
          || std::is_rvalue_reference_v<front_type>
          || std::is_nothrow_constructible_v<std::decay_t<front_type>, raw_front_type>))
  -> std::variant<front_type, objpipe_errc> {
    return invoke_fn_(src_.front());
  }

  constexpr auto pop_front()
  noexcept(noexcept(src_.pop_front()))
  -> objpipe_errc {
    return src_.pop_front();
  }

  constexpr auto try_pull()
  noexcept(noexcept(invoke_fn_(adapt::raw_try_pull(src_))))
  -> std::variant<try_pull_type, objpipe_errc> {
    return invoke_fn_(adapt::raw_try_pull(src_));
  }

  constexpr auto pull()
  noexcept(noexcept(invoke_fn_(adapt::raw_pull(src_))))
  -> std::variant<pull_type, objpipe_errc> {
    return invoke_fn_(adapt::raw_pull(src_));
  }

  template<typename NextFn>
  constexpr auto transform(NextFn&& next_fn) &&
  noexcept(noexcept(
          transform_op<Source, Fn..., NextFn>(
              std::move(src_),
              std::move(fn_).extend(std::forward<NextFn>(next_fn)))))
  -> transform_op<Source, Fn..., NextFn> {
    return transform_op<Source, Fn..., NextFn>(
        std::move(src_),
        std::move(fn_).extend(std::forward<NextFn>(next_fn)));
  }

 private:
  template<typename T>
  constexpr auto invoke_fn_(std::variant<T, objpipe_errc>&& v)
  noexcept(is_nothrow_invocable_v<const fn_type&, decltype(std::get<0>(std::declval<std::variant<T, objpipe_errc>>()))>)
  -> std::variant<
      invoke_result_t<const fn_type&, decltype(std::get<0>(std::declval<std::variant<T, objpipe_errc>>()))>,
      objpipe_errc> {
    if (v.index() == 0)
      return { std::in_place_index<0>, std::invoke(fn_, std::get<0>(std::move(v))) };
    else
      return { std::in_place_index<1>, std::get<1>(std::move(v)) };
  }

  Source src_;
  fn_type fn_;
};


} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_TRANSFORM_OP_H */
