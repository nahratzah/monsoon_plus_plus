#ifndef MONSOON_OVERLOAD_H
#define MONSOON_OVERLOAD_H

#include <type_traits>
#include <utility>
#include <functional>

namespace monsoon {
namespace support {


template<typename Fn>
class overload_base_fnptr_ {
 private:
  using fn_type = std::remove_reference_t<Fn>;

 public:
  template<typename FnFwd>
  constexpr overload_base_fnptr_(FnFwd&& fn)
  noexcept(std::is_nothrow_constructible_v<fn_type, FnFwd&&>)
  : fn_(std::forward<FnFwd>(fn))
  {}

  template<typename... FwdArgs>
  constexpr auto operator()(FwdArgs&&... args) const
  noexcept(noexcept(std::invoke(std::declval<const fn_type&>(), std::declval<FwdArgs>()...)))
  -> decltype(std::invoke(std::declval<const fn_type&>(), std::declval<FwdArgs>()...)) {
    return std::invoke(fn_, std::forward<FwdArgs>(args)...);
  }

  template<typename... FwdArgs>
  constexpr auto operator()(FwdArgs&&... args) &
  noexcept(noexcept(std::invoke(std::declval<fn_type&>(), std::declval<FwdArgs>()...)))
  -> decltype(std::invoke(std::declval<fn_type&>(), std::declval<FwdArgs>()...)) {
    return std::invoke(fn_, std::forward<FwdArgs>(args)...);
  }

  template<typename... FwdArgs>
  constexpr auto operator()(FwdArgs&&... args) &&
  noexcept(noexcept(std::invoke(std::declval<fn_type&&>(), std::declval<FwdArgs>()...)))
  -> decltype(std::invoke(std::declval<fn_type&&>(), std::declval<FwdArgs>()...)) {
    return std::invoke(std::forward<fn_type>(fn_), std::forward<FwdArgs>(args)...);
  }

 private:
  fn_type fn_;
};

template<typename Fn>
class overload_base_fnptr_<Fn&> {
 private:
  using fn_type = Fn;

 public:
  constexpr overload_base_fnptr_(fn_type& fn) noexcept
  : fn_(&fn)
  {}

  template<typename... FwdArgs>
  constexpr auto operator()(FwdArgs&&... args) const
  noexcept(noexcept(std::invoke(std::declval<fn_type&>(), std::declval<FwdArgs>()...)))
  -> decltype(std::invoke(std::declval<fn_type&>(), std::declval<FwdArgs>()...)) {
    return std::invoke(*fn_, std::forward<FwdArgs>(args)...);
  }

 private:
  fn_type* fn_;
};


template<typename Fn>
using overload_base_t = std::conditional_t<
       std::is_pointer_v<Fn>
    || std::is_member_pointer_v<Fn>
    || std::is_reference_v<Fn>,
    overload_base_fnptr_<Fn>,
    Fn>;


template<typename... Fn>
class overload_t
: private overload_base_t<Fn>...
{
 public:
  template<typename... FnInit>
  constexpr overload_t(FnInit&&... fn)
  : overload_base_t<Fn>(std::forward<FnInit>(fn))...
  {}

  using overload_base_t<Fn>::operator()...;
};


} /* namespace monsoon::support */


template<typename... Fn>
constexpr auto overload(Fn&&... fn)
-> support::overload_t<Fn...> {
  return support::overload_t<Fn...>(std::forward<Fn>(fn)...);
}


} /* namespace monsoon */

#endif /* MONSOON_OVERLOAD_H */
