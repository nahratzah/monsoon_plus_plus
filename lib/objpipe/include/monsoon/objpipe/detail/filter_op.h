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

template<typename T>
class filter_store_copy_ {
 public:
  static constexpr bool is_const = false;

  constexpr auto present() const
  noexcept
  -> bool {
    return present_;
  }

  template<typename Source>
  constexpr auto load(Source& src)
  noexcept(noexcept(src.front()))
  -> objpipe_errc {
    assert(!present());

    auto v = src.front();
    if (!v.has_value()) return v.errc();
    val_.emplace(std::move(v).value());
    present_ = true;
    return objpipe_errc::success;
  }

  constexpr auto get()
  noexcept
  -> T&& {
    assert(present_);
    assert(val_.has_value());
    return std::move(*val_);
  }

  constexpr auto reset()
  noexcept
  -> void {
    present_ = false;
  }

  constexpr auto release_lref()
  noexcept
  -> T& {
    assert(present_);
    assert(val_.has_value());

    present_ = false;
    return *val_;
  }

  constexpr auto release_rref()
  noexcept
  -> std::conditional_t<is_const, T&, T&&> {
    assert(present_);
    assert(val_.has_value());

    present_ = false;
    return *std::move(val_);
  }

 private:
  bool present_ = false;
  std::optional<T> val_;
};

template<typename T>
class filter_store_ref_ {
 public:
  static constexpr bool is_const = std::is_const_v<T>;

  constexpr auto present() const
  noexcept
  -> bool {
    return ptr_ != nullptr;
  }

  template<typename Source>
  constexpr auto load(Source& src)
  noexcept(noexcept(src.front()))
  -> objpipe_errc {
    static_assert(std::is_lvalue_reference_v<adapt::front_type<Source>>
        || std::is_rvalue_reference_v<adapt::front_type<Source>>,
        "Front type must be a reference");
    assert(!present());

    auto v = src.front();
    if (!v.has_value()) return v.errc();
    ptr_ = v.value_ptr();
    return objpipe_errc::success;
  }

  constexpr auto get() const
  noexcept
  -> T&& {
    assert(ptr_ != nullptr);
    return std::move(*ptr_);
  }

  constexpr auto reset()
  noexcept
  -> void {
    ptr_ = nullptr;
  }

  constexpr auto release_lref()
  noexcept
  -> T& {
    assert(ptr_ != nullptr);

    T*const rv = ptr_;
    ptr_ = nullptr;
    return *rv;
  }

  constexpr auto release_rref()
  noexcept
  -> std::conditional_t<is_const, T&, T&&> {
    assert(ptr_ != nullptr);

    T*const rv = ptr_;
    ptr_ = nullptr;
    return std::move(*rv);
  }

 private:
  T* ptr_ = nullptr;
};

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

  static constexpr bool is_ref =
      std::is_lvalue_reference_v<adapt::front_type<Source>>
      || std::is_rvalue_reference_v<adapt::front_type<Source>>;
  using store_type = std::conditional_t<
      !std::is_volatile_v<adapt::front_type<Source>>
      && (std::is_lvalue_reference_v<adapt::front_type<Source>>
          || std::is_rvalue_reference_v<adapt::front_type<Source>>),
      filter_store_ref_<std::remove_reference_t<adapt::front_type<Source>>>,
      filter_store_copy_<adapt::value_type<Source>>>;
  static constexpr bool noexcept_load =
      noexcept(std::declval<store_type&>().load(std::declval<Source&>()))
      && noexcept(std::declval<store_type&>().get())
      && noexcept(std::declval<store_type&>().present())
      && noexcept(std::declval<store_type&>().reset())
      && noexcept(std::declval<Source&>().pop_front())
      && noexcept_test;
  static constexpr bool noexcept_release =
      noexcept(std::declval<store_type&>().present())
      && (std::is_lvalue_reference_v<adapt::pull_type<Source>>
          ? noexcept(std::declval<store_type&>().release_lref())
          : noexcept(std::declval<store_type&>().release_rref()))
      && noexcept(std::declval<Source&>().pop_front());

 public:
  template<typename... Init>
  explicit constexpr filter_op(Source&& src, Init&&... init)
  noexcept(std::conjunction_v<std::is_nothrow_move_constructible<Source>, std::is_nothrow_constructible<Pred, Init>...>)
  : src_(std::move(src)),
    pred_(std::forward<Init>(init)...)
  {}

  friend auto swap(filter_op& x, filter_op& y)
  noexcept(std::is_nothrow_swappable_v<Source>
      && std::is_nothrow_swappable_v<store_type>
      && std::is_nothrow_swappable_v<std::tuple<Pred...>>)
  -> void {
    using std::swap;
    swap(x.src_, y.src_);
    swap(x.store_, y.store_);
    swap(x.pred_, y.pred_);
  }

  constexpr auto is_pullable()
  noexcept
  -> bool {
    return store_.present() || src_.is_pullable();
  }

  constexpr auto wait()
  noexcept(noexcept_load)
  -> objpipe_errc {
    return load_();
  }

  constexpr auto front()
  noexcept(noexcept_load
      && noexcept(std::declval<store_type&>().get()))
  -> transport<adapt::front_type<Source>> {
    using result_type = transport<adapt::front_type<Source>>;

    objpipe_errc e = load_();
    if (e != objpipe_errc::success) return result_type(std::in_place_index<1>, e);
    return result_type(std::in_place_index<0>, store_.get());
  }

  constexpr auto pop_front()
  noexcept(noexcept_load
      && noexcept(std::declval<store_type&>().reset())
      && noexcept(std::declval<Source&>().pop_front()))
  -> objpipe_errc {
    objpipe_errc e = load_();
    if (e == objpipe_errc::success) {
      store_.reset();
      e = src_.pop_front();
    }
    return e;
  }

  template<bool Enable = !store_type::is_const
      || std::is_const_v<adapt::try_pull_type<Source>>>
  constexpr auto try_pull()
  noexcept(noexcept(std::declval<store_type&>().present())
      && noexcept_release
      && noexcept(adapt::raw_try_pull(src_))
      && noexcept_test)
  -> std::enable_if_t<Enable,
      transport<adapt::try_pull_type<Source>>> {
    if (store_.present()) return release_<adapt::try_pull_type<Source>>();
    for (;;) {
      transport<adapt::try_pull_type<Source>> v =
          adapt::raw_try_pull(src_);
      if (!v.has_value() || test(v.value()))
        return v;
    }
  }

  template<bool Enable = !store_type::is_const
      || std::is_const_v<adapt::pull_type<Source>>>
  constexpr auto pull()
  noexcept(noexcept(std::declval<store_type&>().present())
      && noexcept_release
      && noexcept(adapt::raw_pull(src_))
      && noexcept_test)
  -> std::enable_if_t<Enable,
      transport<adapt::pull_type<Source>>> {
    if (store_.present()) return release_<adapt::pull_type<Source>>();
    for (;;) {
      transport<adapt::pull_type<Source>> v =
          adapt::raw_pull(src_);
      if (!v.has_value() || test(v.value()))
        return v;
    }
  }

 private:
  constexpr auto load_()
  noexcept(noexcept_load)
  -> objpipe_errc {
    while (!store_.present()) {
      objpipe_errc e = store_.load(src_);
      if (e != objpipe_errc::success) return e;
      if (!test(store_.get())) {
        store_.reset();
        e = src_.pop_front();
        if (e != objpipe_errc::success) return e;
      }
    }
    return objpipe_errc::success;
  }

  template<typename T>
  auto release_()
  noexcept(noexcept_release)
  -> transport<T> {
    using result_type = transport<T>;
    assert(store_.present());

    if constexpr(std::is_lvalue_reference_v<adapt::try_pull_type<Source>>) {
      auto rv = result_type(std::in_place_index<0>, store_.release_lref());
      objpipe_errc e = src_.pop_front();
      if (e != objpipe_errc::success) rv.emplace(std::in_place_index<1>, e);
      return rv;
    } else {
      auto rv = result_type(std::in_place_index<0>, store_.release_rref());
      objpipe_errc e = src_.pop_front();
      if (e != objpipe_errc::success) rv.emplace(std::in_place_index<1>, e);
      return rv;
    }
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
  store_type store_;
  std::tuple<Pred...> pred_;
};


} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_FILTER_OP_H */
