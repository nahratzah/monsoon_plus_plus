#ifndef MONSOON_OBJPIPE_DETAIL_PEEK_OP_H
#define MONSOON_OBJPIPE_DETAIL_PEEK_OP_H

///\file
///\ingroup objpipe_detail

#include <functional>
#include <type_traits>
#include <utility>
#include <monsoon/objpipe/detail/invocable_.h>

namespace monsoon::objpipe::detail {


/**
 * \brief Adapter for a in-place inspecting or modifying functor.
 * \ingroup objpipe_detail
 *
 * \details
 * Adapts a functor, so it's usable with transform.
 *
 * \sa \ref monsoon::objpipe::detail::adapter::peek
 */
template<typename Fn>
class peek_adapter {
 public:
  explicit constexpr peek_adapter(Fn&& fn)
  noexcept(std::is_nothrow_move_constructible_v<Fn>)
  : fn_(std::move(fn))
  {}

  explicit constexpr peek_adapter(const Fn& fn)
  noexcept(std::is_nothrow_copy_constructible_v<Fn>)
  : fn_(fn)
  {}

  template<typename Arg>
  constexpr auto operator()(Arg&& arg) const
  noexcept(is_invocable_v<const Fn&, std::add_lvalue_reference_t<Arg>>
      ? is_nothrow_invocable_v<const Fn&, std::add_lvalue_reference_t<Arg>>
      : is_nothrow_invocable_v<const Fn&,
          std::add_lvalue_reference_t<std::add_lvalue_reference_t<std::remove_cv_t<std::remove_reference_t<Arg>>>>>)
  -> std::conditional_t<is_invocable_v<const Fn&, std::add_lvalue_reference_t<Arg>>,
      std::add_rvalue_reference_t<Arg>,
      std::remove_cv_t<std::remove_reference_t<Arg>>> {
    if constexpr(is_invocable_v<const Fn&, std::add_lvalue_reference_t<Arg>>) {
      std::invoke(fn_, arg);
      return static_cast<std::add_rvalue_reference_t<Arg>>(arg);
    } else {
      std::remove_cv_t<std::remove_reference_t<Arg>> copy_of_arg = std::forward<Arg>(arg);
      std::invoke(fn_, copy_of_arg);
      return copy_of_arg;
    }
  }

 private:
  Fn fn_;
};


} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_PEEK_OP_H */
