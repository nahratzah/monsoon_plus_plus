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
#include <variant>
#include <monsoon/objpipe/detail/adapt.h>
#include <monsoon/objpipe/detail/fwd.h>
#include <monsoon/objpipe/detail/invocable_.h>

namespace monsoon::objpipe::detail {


template<typename Arg, typename Fn0, typename... Fn>
constexpr auto filter_test_(const Arg& v, const Fn0& pred0, const Fn&... preds)
noexcept(noexcept(std::invoke(pred0, v) && resolve_(v, preds...)))
-> bool {
  return std::invoke(pred0, v) && resolve_(v, preds...);
}

template<typename Arg>
constexpr auto filter_test_(const Arg& v)
noexcept
-> bool {
  return true;
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
  constexpr auto load(const Source& src)
  noexcept(noexcept(src.front()))
  -> objpipe_errc {
    static_assert(std::is_lvalue_reference_v<adapt::front_type<Source>>
        || std::is_rvalue_reference_v<adapt::front_type<Source>>,
        "Front type must be a reference");
    assert(present_);

    auto v = src.front();
    if (v.index() != 0) return std::get<1>(v);
    val_.emplace(std::get<0>(std::move(v)));
    return objpipe_errc::success;
  }

  constexpr auto get() const
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
  constexpr auto load(const Source& src)
  noexcept(noexcept(src.front()))
  -> objpipe_errc {
    static_assert(std::is_lvalue_reference_v<adapt::front_type<Source>>
        || std::is_rvalue_reference_v<adapt::front_type<Source>>,
        "Front type must be a reference");
    assert(!present());

    auto v = src.front();
    if (v.index() != 0) return std::get<1>(v);
    ptr_ = std::addressof(std::get<0>(v));
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

 public:
  template<typename... Init>
  explicit constexpr filter_op(Source&& src, Init&&... init)
  noexcept(std::conjunction_v<std::is_nothrow_move_constructible<Source>, std::is_nothrow_constructible<Pred, Init>...>)
  : src_(std::move(src)),
    pred_(std::forward<Init>(init)...)
  {}

  constexpr auto is_pullable() const
  noexcept
  -> bool {
    return store_.present() || src_.is_pullable();
  }

  constexpr auto wait() const
  noexcept(noexcept(src_.wait()))
  -> objpipe_errc {
    while (!store_.present()) {
      objpipe_errc e = store_.load(src_);
      if (e != objpipe_errc::success) return e;
      if (!test(store_.get())) store_.reset();
    }
    return objpipe_errc::success;
  }

  constexpr auto front() const
  noexcept(noexcept(src_.front())
      && noexcept(src_.pop_front())
      && noexcept_test)
  -> std::variant<adapt::front_type<Source>, objpipe_errc> {
    while (!store_.present()) {
      objpipe_errc e = store_.load(src_);
      if (e != objpipe_errc::success) return { std::in_place_index<1>, e };
      if (!test(store_.get())) store_.reset();
    }
    return { std::in_place_index<0>, store_.get() };
  }

  constexpr auto pop_front()
  noexcept(noexcept(src_.pop_front())
      && noexcept(src_.front())
      && noexcept_test)
  -> objpipe_errc {
    while (!store_.present()) {
      objpipe_errc e = store_.load(src_);
      if (e != objpipe_errc::success) return e;
      if (!test(store_.get())) store_.reset();
    }

    store_.reset();
    src_.pop_front();
    return objpipe_errc::success;
  }

  template<bool Enable = !store_type::is_const
      || std::is_const_v<adapt::try_pull_type<Source>>>
  constexpr auto try_pull()
  noexcept(noexcept(adapt::raw_try_pull(src_))
      && noexcept_test)
  -> std::enable_if_t<Enable,
      std::variant<adapt::try_pull_type<Source>, objpipe_errc>> {
    if (store_.present()) {
      if constexpr(std::is_lvalue_reference_v<adapt::try_pull_type<Source>>)
        return { std::in_place_index<0>, store_.release_lref() };
      else
        return { std::in_place_index<0>, store_.release_rref() };
    }

    for (;;) {
      std::variant<adapt::try_pull_type<Source>, objpipe_errc> v =
          adapt::raw_try_pull(src_);
      if (v.index() != 0 || test(std::get<0>(v)))
        return v;
    }
  }

  template<bool Enable = !store_type::is_const
      || std::is_const_v<adapt::pull_type<Source>>>
  constexpr auto pull()
  noexcept(noexcept(adapt::raw_pull(src_))
      && noexcept_test)
  -> std::enable_if_t<Enable,
      std::variant<adapt::pull_type<Source>, objpipe_errc>> {
    if (store_.present()) {
      if constexpr(std::is_lvalue_reference_v<adapt::pull_type<Source>>)
        return { std::in_place_index<0>, store_.release_lref() };
      else
        return { std::in_place_index<0>, store_.release_rref() };
    }

    for (;;) {
      std::variant<adapt::pull_type<Source>, objpipe_errc> v =
          adapt::raw_pull(src_);
      if (v.index() != 0 || test(std::get<0>(v)))
        return v;
    }
  }

 private:
  template<size_t... Idx>
  constexpr auto test(cref v,
      std::index_sequence<Idx...> = std::index_sequence_for<Pred...>()) const
  noexcept(noexcept_test)
  -> bool {
    return filter_test_(v, std::get<Idx>(pred_)...);
  }

  mutable Source src_;
  mutable store_type store_;
  std::tuple<Pred...> pred_;
};


} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_FILTER_OP_H */
