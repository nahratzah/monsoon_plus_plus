#ifndef MONSOON_OBJPIPE_DETAIL_INVOCABLE__H
#define MONSOON_OBJPIPE_DETAIL_INVOCABLE__H

///\file
///\ingroup objpipe_detail

#include <type_traits>
#include <functional>

namespace monsoon::objpipe::detail {


#if __cpp_lib_is_invocable
using std::is_invocable;
using std::is_nothrow_invocable;
using std::invoke_result;
using std::is_invocable_v;
using std::is_nothrow_invocable_v;
using std::invoke_result_t;
#else
template<typename... Args>
struct arg_pack_ {};

template<typename Fn, typename ArgPack, typename = void>
struct invoke_result_ {};

template<typename Fn, typename... Args>
struct invoke_result_<Fn, arg_pack_<Args...>, std::void_t<decltype(std::invoke(std::declval<Fn>(), std::declval<Args>()...))>> {
  using type = decltype(std::invoke(std::declval<Fn>(), std::declval<Args>()...));
};

template<typename Fn, typename... Args>
using invoke_result = invoke_result_<Fn, arg_pack_<Args...>>;

template<typename Fn, typename... Args>
using invoke_result_t = typename invoke_result<Fn, Args...>::type;

template<typename Fn, typename ArgPack, typename = void>
struct is_invocable_
: std::false_type
{};

template<typename Fn, typename... Args>
struct is_invocable_<Fn, arg_pack_<Args...>, std::void_t<invoke_result_t<Fn, Args...>>>
: std::true_type
{};

template<typename Fn, typename... Args>
using is_invocable = typename is_invocable_<Fn, arg_pack_<Args...>>::type;

template<typename Fn, typename... Args>
constexpr bool is_invocable_v = is_invocable<Fn, Args...>::value;

template<typename Fn, typename ArgPack, bool = is_invocable_<Fn, ArgPack>::value>
struct is_nothrow_invocable_
: std::false_type
{};

template<typename Fn, typename... Args>
struct is_nothrow_invocable_<Fn, arg_pack_<Args...>, true>
: std::integral_constant<bool, noexcept(std::invoke(std::declval<Fn>(), std::declval<Args>()...))>
{};

template<typename Fn, typename... Args>
using is_nothrow_invocable = typename is_nothrow_invocable_<Fn, arg_pack_<Args...>>::type;

template<typename Fn, typename... Args>
constexpr bool is_nothrow_invocable_v = is_nothrow_invocable<Fn, Args...>::value;
#endif


} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_INVOCABLE__H */
