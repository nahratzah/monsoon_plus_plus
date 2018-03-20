#ifndef MONSOON_OBJPIPE_DETAIL_FILTER_OP_H
#define MONSOON_OBJPIPE_DETAIL_FILTER_OP_H

///\file
///\ingroup objpipe_detail

#include <functional>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <monsoon/objpipe/detail/adapt.h>
#include <monsoon/objpipe/detail/fwd.h>
#include <monsoon/objpipe/detail/invocable_.h>
#include <monsoon/objpipe/detail/transport.h>

namespace monsoon::objpipe::detail {


template<typename Arg, typename Fn0, typename... Fn>
constexpr auto filter_test_(const Arg& v, const Fn0& pred0, const Fn&... preds)
noexcept(std::conjunction_v<
    is_nothrow_invocable<const Fn0&, const Arg&>,
    is_nothrow_invocable<const Fn&, const Arg&>...>)
-> bool {
  if (!std::invoke(pred0, v)) return false;
  if constexpr(sizeof...(preds) == 0)
    return true;
  else
    return filter_test_(v, preds...);
}

/**
 * \brief Filter operation.
 * \ingroup objpipe_detail
 *
 * \details
 * Filters values based on predicates.
 *
 * \tparam Source The nested source.
 * \tparam Pred The predicates to check.
 * \sa \ref monsoon::objpipe::detail::adapter::filter
 */
template<typename Source, typename... Pred>
class filter_op {
 private:
  using cref = std::add_lvalue_reference_t<std::add_const_t<adapt::value_type<Source>>>;
  static constexpr bool noexcept_test =
      std::conjunction_v<is_nothrow_invocable<const Pred&, cref>...>;

  using store_type = transport<adapt::front_type<Source>>;
  static constexpr bool noexcept_load =
      std::is_nothrow_move_assignable_v<store_type>
      && noexcept(std::declval<Source&>().front())
      && noexcept(std::declval<Source&>().pop_front())
      && noexcept_test;

 public:
  template<typename... Init>
  explicit constexpr filter_op(Source&& src, Init&&... init)
  noexcept(std::conjunction_v<std::is_nothrow_move_constructible<Source>, std::is_nothrow_constructible<Pred, Init>...>)
  : src_(std::move(src)),
    pred_(std::forward<Init>(init)...)
  {}

  filter_op(const filter_op&) = delete;

  constexpr filter_op(filter_op&&)
  noexcept(std::conjunction_v<std::is_nothrow_move_constructible<Source>, std::is_nothrow_move_constructible<Pred>...>) = default;

  filter_op& operator=(const filter_op&) = delete;
  filter_op& operator=(filter_op&&) = delete;

  constexpr auto is_pullable()
  noexcept
  -> bool {
    return store_.has_value()
        || (store_.errc() != objpipe_errc::success
            && store_.errc() != objpipe_errc::closed)
        || src_.is_pullable();
  }

  constexpr auto wait()
  noexcept(noexcept_load)
  -> objpipe_errc {
    return load_();
  }

  constexpr auto front()
  noexcept(noexcept_load
      && std::is_nothrow_move_constructible_v<store_type>)
  -> store_type {
    using result_type = transport<adapt::front_type<Source>>;

    objpipe_errc e = load_();
    if (e != objpipe_errc::success) return result_type(std::in_place_index<1>, e);
    return std::move(store_);
  }

  constexpr auto pop_front()
  noexcept(noexcept_load
      && noexcept(std::declval<store_type&>().emplace(std::in_place_index<1>, std::declval<objpipe_errc>()))
      && noexcept(std::declval<Source&>().pop_front()))
  -> objpipe_errc {
    objpipe_errc e = load_();
    if (e == objpipe_errc::success) {
      e = src_.pop_front();
      store_.emplace(std::in_place_index<1>, e);
    }
    return e;
  }

  template<bool Enable =
      adapt::has_try_pull<Source>
      && (std::is_same_v<store_type, transport<adapt::try_pull_type<Source>>>
          || std::is_same_v<transport<adapt::value_type<Source>>, transport<adapt::try_pull_type<Source>>>)>
  constexpr auto try_pull()
  noexcept(
      std::is_nothrow_constructible_v<transport<adapt::try_pull_type<Source>>, store_type>
      && noexcept(std::declval<Source&>().pop_front())
      && noexcept(adapt::raw_try_pull(src_))
      && noexcept_test)
  -> std::enable_if_t<Enable,
      transport<adapt::try_pull_type<Source>>> {
    if (store_.errc() != objpipe_errc::success)
      return transport<adapt::try_pull_type<Source>>(std::in_place_index<1>, store_.errc());

    if (store_.has_value()) {
      transport<adapt::try_pull_type<Source>> result = std::move(store_);
      store_.emplace(std::in_place_index<1>, src_.pop_front());
      if (store_.errc() != objpipe_errc::success)
        result.emplace(std::in_place_index<1>, store_.errc());
      return result;
    }

    for (;;) {
      transport<adapt::try_pull_type<Source>> v =
          adapt::raw_try_pull(src_);
      store_.emplace(std::in_place_index<1>, v.errc());
      if (!v.has_value() || test(v.value()))
        return v;
    }
  }

  template<bool Enable =
      adapt::has_pull<Source>
      && (std::is_same_v<store_type, transport<adapt::pull_type<Source>>>
          || std::is_same_v<transport<adapt::value_type<Source>>, transport<adapt::pull_type<Source>>>)>
  constexpr auto pull()
  noexcept(
      std::is_nothrow_constructible_v<transport<adapt::pull_type<Source>>, store_type>
      && noexcept(std::declval<Source&>().pop_front())
      && noexcept(adapt::raw_pull(src_))
      && noexcept_test)
  -> std::enable_if_t<Enable,
      transport<adapt::pull_type<Source>>> {
    if (store_.errc() != objpipe_errc::success)
      return transport<adapt::try_pull_type<Source>>(std::in_place_index<1>, store_.errc());

    if (store_.has_value()) {
      transport<adapt::try_pull_type<Source>> result = std::move(store_);
      store_.emplace(std::in_place_index<1>, src_.pop_front());
      if (store_.errc() != objpipe_errc::success)
        result.emplace(std::in_place_index<1>, store_.errc());
      return result;
    }

    for (;;) {
      transport<adapt::pull_type<Source>> v =
          adapt::raw_pull(src_);
      store_.emplace(std::in_place_index<1>, v.errc());
      if (!v.has_value() || test(v.value()))
        return v;
    }
  }

 private:
  constexpr auto load_()
  noexcept(noexcept_load)
  -> objpipe_errc {
    while (!store_.has_value() && store_.errc() == objpipe_errc::success) {
      store_ = src_.front();
      assert(store_.has_value() || store_.errc() != objpipe_errc::success);

      if (store_.errc() == objpipe_errc::success && !test(store_.ref()))
        store_.emplace(std::in_place_index<1>, src_.pop_front());
    }
    return store_.errc();
  }

  constexpr auto test(cref v) const
  noexcept(noexcept_test)
  -> bool {
    return test(v, std::index_sequence_for<Pred...>());
  }

  template<std::size_t... Idx>
  constexpr auto test(cref v,
      std::index_sequence<Idx...>) const
  noexcept(noexcept_test)
  -> bool {
    return filter_test_(v, std::get<Idx>(pred_)...);
  }

  Source src_;
  store_type store_ = store_type(std::in_place_index<1>, objpipe_errc::success);
  std::tuple<Pred...> pred_;
};


} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_FILTER_OP_H */
