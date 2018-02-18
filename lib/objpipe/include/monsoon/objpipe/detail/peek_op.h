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
 private:
  template<typename Arg>
  static constexpr bool is_cref_invocable = is_invocable_v<const Fn&, std::add_lvalue_reference_t<std::add_const_t<Arg>>>;
  template<typename Arg>
  static constexpr bool is_lref_invocable = is_invocable_v<const Fn&, std::add_lvalue_reference_t<Arg>>;

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
  constexpr auto operator()(const Arg& arg) const
  noexcept(is_nothrow_invocable_v<const Fn&, const Arg&>)
  -> std::enable_if_t<is_cref_invocable<Arg>, const Arg&> {
    std::invoke(fn_, arg);
    return arg;
  }

  template<typename Arg>
  constexpr auto operator()(const Arg& arg) const
  noexcept(std::is_nothrow_copy_constructible_v<Arg>
      && is_nothrow_invocable_v<const Fn&, std::add_lvalue_reference_t<Arg>>)
  -> std::enable_if_t<!is_cref_invocable<Arg> && is_lref_invocable<Arg>, Arg> {
    Arg tmp = arg;
    std::invoke(fn_, arg);
    return tmp;
  }

  template<typename Arg>
  constexpr auto operator()(Arg&& arg) const
  noexcept(is_nothrow_invocable_v<const Fn&, std::add_lvalue_reference_t<Arg>>)
  -> std::enable_if_t<!std::is_const_v<Arg> && is_lref_invocable<std::remove_reference_t<Arg>>,
      std::add_rvalue_reference_t<Arg>> {
    std::invoke(fn_, arg);
    return arg;
  }

 private:
  Fn fn_;
};


} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_PEEK_OP_H */
