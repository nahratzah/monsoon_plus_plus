#ifndef MONSOON_OBJPIPE_DETAIL_DEREF_OP_H
#define MONSOON_OBJPIPE_DETAIL_DEREF_OP_H

#include <type_traits>
#include <utility>
#include <monsoon/objpipe/detail/invocable_.h>

namespace monsoon::objpipe::detail {


///\brief Implements the dereference operator for T.
///\details Simply invokes operator* on an instance of T.
struct deref_op {
  template<typename T>
  static constexpr bool is_valid = is_invocable_v<deref_op, const T&>
      || is_invocable_v<deref_op, T&>
      || is_invocable_v<deref_op, T&&>;

  template<typename T>
  constexpr auto operator()(T&& v) const
  noexcept(noexcept(*std::declval<T>()))
  -> decltype(*std::declval<T>()) {
    return static_cast<decltype(*std::declval<T>())>(*v);
  }
};


} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_DEREF_OP_H */
