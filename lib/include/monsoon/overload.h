#ifndef MONSOON_OVERLOAD_H
#define MONSOON_OVERLOAD_H

#include <type_traits>
#include <utility>

namespace monsoon {
inline namespace support {


template<typename> class overload_base_fnptr_;

template<typename Fn, typename... Args>
class overload_base_fnptr_<Fn(Args...)> {
 private:
  using fn_type = Fn(Args...);

 public:
  constexpr overload_base_fnptr_(fn_type* fn)
  : fn_(fn)
  {}

  constexpr decltype(auto) operator()(Args... args) const {
    return std::invoke(fn_, std::forward<Args>(args)...);
  }

 private:
  fn_type* fn_;
};

template<typename Fn>
using overload_base_t = std::conditional_t<
    std::is_pointer_v<Fn>,
    overload_base_fnptr_<std::remove_pointer_t<Fn>>,
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

template<typename... Fn>
constexpr auto overload(Fn&&... fn)
-> overload_t<std::decay_t<Fn>...> {
  return overload_t<std::decay_t<Fn>...>(std::forward<Fn>(fn)...);
}


}} /* namespace monsoon::(inline)support */

#endif /* MONSOON_OVERLOAD_H */
