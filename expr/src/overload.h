#ifndef OVERLOAD_H
#define OVERLOAD_H

#include <type_traits>
#include <utility>

namespace monsoon {
namespace expressions {
inline namespace support {


template<typename... Fn>
class overload_t
: private Fn...
{
 public:
  template<typename... FnInit>
  constexpr overload_t(FnInit&&... fn)
  : Fn(std::forward<FnInit>(fn))...
  {}

  using Fn::operator()...;
};

template<typename... Fn>
constexpr auto overload(Fn&&... fn)
-> overload_t<std::decay_t<Fn>...> {
  return overload_t<std::decay_t<Fn>...>(std::forward<Fn>(fn)...);
}


}}} /* namespace monsoon::expressions::(inline)support */

#endif /* OVERLOAD_H */
