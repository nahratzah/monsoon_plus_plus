#ifndef MONSOON_CACHE_WEAKEN_DECORATOR_H
#define MONSOON_CACHE_WEAKEN_DECORATOR_H

#include <monsoon/cache/element.h>

namespace monsoon::cache {


struct weaken_decorator {
  template<typename Builder>
  constexpr weaken_decorator([[maybe_unused]] const Builder& b) noexcept
  {}

  template<typename T, typename... D>
  auto on_create(element<T, D...>& elem) const noexcept
  -> void {
    elem.weaken();
  }
};


} /* namespace monsoon::cache */

#endif /* MONSOON_CACHE_WEAKEN_DECORATOR_H */
