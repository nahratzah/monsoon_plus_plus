#ifndef _ILIAS_ANY_INL_H_
#define _ILIAS_ANY_INL_H_

#include "any.h"
#include <memory>
#include <string>

namespace monsoon {
namespace impl {


template<size_t Idx, size_t End>
struct recursive_visit {
 private:
  using next = recursive_visit<Idx + 1, End>;

 public:
  template<typename Any, typename Fn0, typename... Fn>
  void operator()(Any&& a, Fn0&& fn0, Fn&&... fn) {
    if (Idx == a.selector()) {
      _namespace(std)::forward<Fn0>(fn0)(
          get<Idx>(_namespace(std)::forward<Any>(a)));
    }
    next()(_namespace(std)::forward<Any>(a),
           _namespace(std)::forward<Fn>(fn)...);
  }
};
/* Tail node specialization. */
template<size_t End>
struct recursive_visit<End, End> {
  template<typename Any>
  void operator()(Any&&) {}
};


template<size_t Idx, size_t End>
struct recursive_map {
 private:
  using next = recursive_map<Idx + 1, End>;

 public:
  template<typename Any, typename Fn0, typename... Fn>
  auto operator()(Any&& a, Fn0&& fn0, Fn&&... fn) ->
      decltype(next()(map<Idx>(forward<Any>(a), forward<Fn0>(fn0)),
                      forward<Fn>(fn)...)) {
    using _namespace(std)::forward;

    return next()(map<Idx>(forward<Any>(a), forward<Fn0>(fn0)),
                  forward<Fn>(fn)...);
  }
};
/* Tail node specialization. */
template<size_t End>
struct recursive_map<End, End> {
  template<typename Any>
  Any operator()(Any&& a) {
    return a;
  }
};


template<typename Result, size_t Idx, size_t End>
struct recursive_map_onto {
 private:
  using next = recursive_map_onto<Result, Idx + 1, End>;

 public:
  template<typename Any, typename Fn0, typename... Fn>
  auto operator()(Any&& a, Fn0&& fn0, Fn&&... fn) -> Result {
    using _namespace(std)::forward;

    if (Idx == a.selector())
      return fn0(get<Idx>(forward<Any>(a)));
    return next()(forward<Any>(a), forward<Fn>(fn)...);
  }
};
/* Tail node specialization. */
template<typename Result, size_t End>
struct recursive_map_onto<Result, End, End> {
  template<typename Any>
  Result operator()(Any&&) {
    assert_msg(false, "Should be unreachable.");
    throw logic_error("ilias::impl::recursive_map_onto: "
                      "base case should be unreachable.");
  }
};


template<typename, size_t, size_t, size_t> struct copy_skip;  // Unimplemented
template<typename... T, size_t Idx, size_t Skip, size_t End>
struct copy_skip<any<T...>, Idx, Skip, End> {
  template<typename InAny>
  any<T...> operator()(InAny&& v) {
    if (Idx == v.selector())
      return any<T...>::template create<Idx>(get<Idx>(forward<InAny>(v)));
    return copy_skip<any<T...>, Idx + 1, Skip, End>()(forward<InAny>(v));
  }
};
template<typename... T, size_t Idx, size_t End>
struct copy_skip<any<T...>, Idx, Idx, End>
: copy_skip<any<T...>, Idx + 1, Idx, End>
{};
template<typename... T, size_t Skip, size_t End>
struct copy_skip<any<T...>, End, Skip, End> {
  template<typename InAny>
  any<T...> operator()(InAny&&) {
    assert_msg(false, "Should be unreachable.");
    throw logic_error("ilias::impl::copy_skip: "
                      "base case should be unreachable.");
  }
};


template<typename T>
auto dereference_any_value(const any_decorate<T>* v) ->
    enable_if_t<is_reference<T>::value, T> {
  return **v;
}

template<typename T>
auto dereference_any_value(const any_decorate<T>* v) ->
    enable_if_t<!is_reference<T>::value,
                add_lvalue_reference_t<add_const_t<T>>> {
  return *v;
}

template<typename T>
auto dereference_any_value(any_decorate<T>* v) ->
    enable_if_t<!is_reference<T>::value,
                add_lvalue_reference_t<T>> {
  return *v;
}

template<typename T>
auto dereference_any_rvalue(any_decorate<T>* v) ->
    enable_if_t<is_lvalue_reference<T>::value, T> {
  return **v;
}

template<typename T>
auto dereference_any_rvalue(any_decorate<T>* v) ->
    enable_if_t<!is_lvalue_reference<T>::value, add_rvalue_reference_t<T>> {
  return move(*v);
}


template<typename, typename...> struct count_type_;  // Specializations only.
template<typename Sel, typename T0, typename... T>
struct count_type_<Sel, T0, T...> {
  static constexpr size_t self_value = (is_same<Sel, T0>::value ? 1U : 0U);
  static constexpr size_t value = self_value + count_type_<Sel, T...>::value;
};
template<typename Sel>
struct count_type_<Sel> {
  static constexpr size_t self_value = 0;
  static constexpr size_t value = 0;
};
template<typename Sel, typename... T> using is_ambiguous_type =
    integral_constant<bool, (count_type_<Sel, T...>::value > 1)>;


template<size_t, size_t, typename...> struct deleter;  // Specializations only
template<size_t Idx, size_t End, typename T0, typename... T>
struct deleter<Idx, End, T0, T...> {
  deleter<Idx + 1, End, T...> next_;

  void operator()(size_t n, void* vptr) {
    if (n == Idx)
      static_cast<T0*>(vptr)->~T0();
    else
      next_(n, vptr);
  }
};
template<size_t Idx, size_t End, typename T0, typename... T>
struct deleter<Idx, End, T0&, T...> {
  deleter<Idx + 1, End, T...> next_;

  void operator()(size_t n, void* vptr) {
    if (n != Idx)
      next_(n, vptr);
  }
};
template<size_t End>
struct deleter<End, End> {
  void operator()(size_t, void*) {}
};


template<typename Sel, typename... T> struct any_constructor_resolution;
template<typename Sel, typename T0, typename... T>
struct any_constructor_resolution<Sel, T0, T...> {
 private:
  using tail = any_constructor_resolution<Sel, T...>;
  using cleaned_sel = remove_reference_t<remove_const_t<Sel>>;
  using cleaned_t0 = remove_reference_t<remove_const_t<T0>>;

  static constexpr bool exact_match = is_same<cleaned_sel, cleaned_t0>::value;
  static constexpr bool inexact_match =
      is_assignable<cleaned_t0, cleaned_sel>::value;

 public:
  static constexpr size_t found_exact =
      (exact_match ? 1U : 0U) +
      tail::found_exact;
  static constexpr size_t found_inexact =
      (inexact_match ? 1U : 0U) +
      tail::found_inexact;

  static constexpr bool found = (found_exact == 1 ||
                                 (found_exact == 0 && found_inexact == 1));
  static constexpr size_t idx =
      ((found_exact >= 1 && exact_match) ||
       (found_exact == 0 && found_inexact >= 1 && inexact_match) ?
       0U :
       tail::idx + 1U);
};
/* Base case. */
template<typename Sel>
struct any_constructor_resolution<Sel> {
  static constexpr size_t found_exact = 0;
  static constexpr size_t found_inexact = 0;
  static constexpr size_t idx = 0;
};


template<typename T0, typename... T>
struct is_nothrow_move_constructible_any_<T0, T...> {
  static constexpr bool value =
      is_nothrow_move_constructible<T0>::value &&
      is_nothrow_move_constructible_any_<T...>::value;
};
template<>
struct is_nothrow_move_constructible_any_<> {
  static constexpr bool value = true;
};


template<typename T0, typename... T>
struct is_nothrow_copy_constructible_any_<T0, T...> {
  static constexpr bool value =
      is_nothrow_copy_constructible<T0>::value &&
      is_nothrow_copy_constructible_any_<T...>::value;
};
template<>
struct is_nothrow_copy_constructible_any_<> {
  static constexpr bool value = true;
};


template<typename T0, typename... T>
struct is_nothrow_move_assignable_any_<T0, T...> {
  static constexpr bool value =
      is_nothrow_move_assignable<T0>::value &&
      is_nothrow_move_assignable_any_<T...>::value;
};
template<>
struct is_nothrow_move_assignable_any_<> {
  static constexpr bool value = true;
};


template<typename T0, typename... T>
struct is_nothrow_copy_assignable_any_<T0, T...> {
  static constexpr bool value =
      is_nothrow_copy_assignable<T0>::value &&
      is_nothrow_copy_assignable_any_<T...>::value;
};
template<>
struct is_nothrow_copy_assignable_any_<> {
  static constexpr bool value = true;
};


template<size_t Idx, size_t End, typename T0, typename... T>
struct copy_operation<Idx, End, T0, T...> {
  copy_operation<Idx + 1, End, T...> next;

  template<typename AnyIn>
  size_t operator()(void* vptr, AnyIn&& other, size_t trivial_size) {
    using origin_type = decltype(get<Idx>(forward<AnyIn>(other)));

    if (!is_trivially_constructible<any_decorate<T0>, origin_type>::value &&
        Idx == other.selector()) {
      new (vptr) any_decorate<T0>(get<Idx>(forward<AnyIn>(other)));
      return Idx;
    } else {
      return next(vptr, forward<AnyIn>(other), trivial_size);
    }
  }
};
template<size_t Idx, size_t End, typename T0, typename... T>
struct copy_operation<Idx, End, T0&, T...> {
  copy_operation<Idx + 1, End, T...> next;

  template<typename AnyIn>
  size_t operator()(void* vptr, AnyIn&& other, size_t trivial_size) {
    using origin_type = decltype(&get<Idx>(forward<AnyIn>(other)));

    if (!is_trivially_constructible<any_decorate<T0&>, origin_type>::value &&
        Idx == other.selector()) {
      new (vptr) any_decorate<T0&>(&get<Idx>(forward<AnyIn>(other)));
      return Idx;
    } else {
      return next(vptr, forward<AnyIn>(other), trivial_size);
    }
  }
};
template<size_t End>
struct copy_operation<End, End> {
  template<typename AnyIn>
  size_t operator()(void* vptr, AnyIn&& other, size_t trivial_size) {
    __builtin_memcpy(vptr, &get<1>(other.data_),
                     min(sizeof(get<1>(other.data_)), trivial_size));
    return other.selector();
  }
};

/* For references, use the address. */
template<typename T, typename U>
auto any_decorate_reference(U&& u) noexcept ->
    enable_if_t<is_lvalue_reference<T>::value,
                add_pointer_t<remove_reference_t<U>>> {
  return addressof(u);
}
/* For values, use the same logic as std::forward. */
template<typename T, typename U>
auto any_decorate_reference(U&& u) noexcept ->
    enable_if_t<!is_lvalue_reference<T>::value,
                U&&> {
  return static_cast<U&&>(u);
}


} /* namespace ilias::impl */


template<typename... T>
constexpr any<T...>::any() noexcept
: data_(sizeof...(T), {})
{}

template<typename... T>
any<T...>::any(const any& other)
    noexcept(impl::is_nothrow_copy_constructible_any<T...>()) {
  impl::copy_operation<0, sizeof...(T), T...> op;

  _namespace(std)::get<0>(data_) = op(&_namespace(std)::get<1>(data_), other,
                                      sizeof(_namespace(std)::get<1>(data_)));
}

template<typename... T>
any<T...>::any(any&& other)
    noexcept(impl::is_nothrow_move_constructible_any<T...>()) {
  impl::copy_operation<0, sizeof...(T), T...> op;

  _namespace(std)::get<0>(data_) = op(&_namespace(std)::get<1>(data_),
                                      _namespace(std)::move(other),
                                      sizeof(_namespace(std)::get<1>(data_)));
}

template<typename... T>
auto any<T...>::operator=(const any& other)
    noexcept(impl::is_nothrow_copy_assignable_any<T...>()) -> any& {
  impl::copy_operation<0, sizeof...(T), T...> op;

  impl::deleter<0, sizeof...(T), T...>()(_namespace(std)::get<0>(data_),
                                         &_namespace(std)::get<1>(data_));
  get<0>(data_) = sizeof...(T);
  _namespace(std)::get<0>(data_) = op(&_namespace(std)::get<1>(data_),
                                      other,
                                      sizeof(_namespace(std)::get<1>(data_)));
  return *this;
}

template<typename... T>
auto any<T...>::operator=(any&& other)
    noexcept(impl::is_nothrow_move_assignable_any<T...>()) -> any& {
  impl::copy_operation<0, sizeof...(T), T...> op;

  impl::deleter<0, sizeof...(T), T...>()(_namespace(std)::get<0>(data_),
                                         &_namespace(std)::get<1>(data_));
  get<0>(data_) = sizeof...(T);
  _namespace(std)::get<0>(data_) = op(&_namespace(std)::get<1>(data_),
                                      _namespace(std)::move(other),
                                      sizeof(_namespace(std)::get<1>(data_)));
  return *this;
}

template<typename... T>
any<T...>::~any() noexcept {
  impl::deleter<0, sizeof...(T), T...>()(_namespace(std)::get<0>(data_),
                                         &_namespace(std)::get<1>(data_));
}

template<typename... T>
template<typename U, size_t N>
any<T...>::any(U&& v) {
  using type = impl::any_decorate<impl::select_n_t<N, T...>>;
  using impl::any_decorate_reference;
  using impl::select_n_t;
  using _namespace(std)::forward;

  void* vptr = &_namespace(std)::get<1>(data_);
  new (vptr) type(any_decorate_reference<select_n_t<N, T...>>(forward<U>(v)));
  _namespace(std)::get<0>(data_) = N;
}

template<typename... T>
template<size_t N>
auto any<T...>::create(impl::select_n_t<N, T...> v) ->
    _namespace(std)::enable_if_t<_namespace(std)::is_lvalue_reference<
                                     impl::select_n_t<N, T...>>::value,
                                 any> {
  using type = impl::any_decorate<impl::select_n_t<N, T...>>;

  any rv;
  void* vptr = &_namespace(std)::get<1>(rv.data_);
  new (vptr) type(&v);
  _namespace(std)::get<0>(rv.data_) = N;
  return rv;
}

template<typename... T>
template<size_t N>
auto any<T...>::create(impl::select_n_t<N, T...> v) ->
    _namespace(std)::enable_if_t<!_namespace(std)::is_lvalue_reference<
                                      impl::select_n_t<N, T...>>::value,
                                 any> {
  using type = impl::any_decorate<impl::select_n_t<N, T...>>;

  any rv;
  void* vptr = &_namespace(std)::get<1>(rv.data_);
  new (vptr) type(v);
  _namespace(std)::get<0>(rv.data_) = N;
  return rv;
}


inline any_error::any_error(_namespace(std)::string_ref msg)
: _namespace(std)::runtime_error(msg)
{}

inline any_error::any_error(const _namespace(std)::string& msg)
: _namespace(std)::runtime_error(msg)
{}

inline any_error::any_error(const char* msg)
: _namespace(std)::runtime_error(msg)
{}


template<size_t N, typename... T>
auto get(any<T...>& v) ->
    std::add_lvalue_reference_t<impl::select_n_t<N, T...>> {
  using base_type = impl::select_n_t<N, T...>;
  using type = impl::any_decorate<base_type>;

  if (_predict_false(N != v.selector())) any_error::__throw();
  void* vptr = &_namespace(std)::get<1>(v.data_);
  return impl::dereference_any_value<base_type>(static_cast<type*>(vptr));
}

template<size_t N, typename... T>
auto get(const any<T...>& v) ->
    std::add_lvalue_reference_t<std::add_const_t<
        impl::select_n_t<N, T...>>> {
  using base_type = impl::select_n_t<N, T...>;
  using type = impl::any_decorate<base_type>;

  if (_predict_false(N != v.selector())) any_error::__throw();
  const void* vptr = &_namespace(std)::get<1>(v.data_);
  return impl::dereference_any_value<base_type>(
      static_cast<const type*>(vptr));
}

template<size_t N, typename... T>
auto get(any<T...>&& v) ->
    std::add_rvalue_reference_t<impl::select_n_t<N, T...>> {
  using base_type = impl::select_n_t<N, T...>;
  using type = impl::any_decorate<base_type>;

  if (_predict_false(N != v.selector())) any_error::__throw();
  void* vptr = &_namespace(std)::get<1>(v.data_);
  return impl::dereference_any_rvalue<base_type>(static_cast<type*>(vptr));
}


template<typename Sel, typename... T>
auto get(any<T...>& v) ->
    decltype(get<impl::type_to_index<Sel, T...>::value>(
                 _namespace(std)::declval<any<T...>&>())) {
  static_assert(impl::is_ambiguous_type<Sel, T...>(),
                "Type selection is ambiguous.");
  return get<impl::type_to_index<Sel, T...>::value>(v);
}

template<typename Sel, typename... T>
auto get(const any<T...>& v) ->
    decltype(get<impl::type_to_index<Sel, T...>::value>(
                 _namespace(std)::declval<const any<T...>&>())) {
  static_assert(impl::is_ambiguous_type<Sel, T...>(),
                "Type selection is ambiguous.");
  return get<impl::type_to_index<Sel, T...>::value>(v);
}

template<typename Sel, typename... T>
auto get(any<T...>&& v) ->
    decltype(get<impl::type_to_index<Sel, T...>::value>(
                 _namespace(std)::declval<any<T...>>())) {
  static_assert(impl::is_ambiguous_type<Sel, T...>(),
                "Type selection is ambiguous.");
  return get<impl::type_to_index<Sel, T...>::value>(v);
}


template<size_t N, typename... T>
auto get_optional(const any<T...>& v) ->
    optional<impl::select_n_t<N, T...>> {
  using type = optional<impl::select_n_t<N, T...>>;

  if (N != v.selector()) return type();
  return get<N>(v);
}

template<size_t N, typename... T>
auto get_optional(any<T...>&& v) ->
    optional<impl::select_n_t<N, T...>> {
  using type = optional<impl::select_n_t<N, T...>>;

  if (N != v.selector()) return type();
  return get<N>(_namespace(std)::move(v));
}


template<size_t N, typename... T, typename Fn>
auto visit(any<T...>& v, Fn fn) -> bool {
  if (N != v.selector()) return false;
  fn(get<N>(v));
  return true;
}

template<size_t N, typename... T, typename Fn>
auto visit(const any<T...>& v, Fn fn) -> bool {
  if (N != v.selector()) return false;
  fn(get<N>(v));
  return true;
}

template<size_t N, typename... T, typename Fn>
auto visit(any<T...>&& v, Fn fn) -> bool {
  if (N != v.selector()) return false;
  fn(get<N>(v));
  return true;
}


template<typename... T, typename... Fn>
auto visit(any<T...>& v, Fn&&... fn) ->
    _namespace(std)::enable_if_t<sizeof...(T) == sizeof...(Fn), void> {
  impl::recursive_visit<0, sizeof...(T)>()(
      v, _namespace(std)::forward<Fn>(fn)...);
}

template<typename... T, typename... Fn>
auto visit(const any<T...>& v, Fn&&... fn) ->
    _namespace(std)::enable_if_t<sizeof...(T) == sizeof...(Fn), void> {
  impl::recursive_visit<0, sizeof...(T)>()(
      v, _namespace(std)::forward<Fn>(fn)...);
}

template<typename... T, typename... Fn>
auto visit(any<T...>&& v, Fn&&... fn) ->
    _namespace(std)::enable_if_t<sizeof...(T) == sizeof...(Fn), void> {
  impl::recursive_visit<0, sizeof...(T)>()(
      move(v), _namespace(std)::forward<Fn>(fn)...);
}


template<size_t N, typename... T, typename Fn>
auto map(const any<T...>& v, Fn fn) ->
    impl::replace_type<
        N,
        decltype(_namespace(std)::declval<Fn>()(
                     get<N>(_namespace(std)::declval<const any<T...>&>()))),
        any<T...>> {
  using type = impl::replace_type<
      N,
      decltype(_namespace(std)::declval<Fn>()(
                   get<N>(_namespace(std)::declval<const any<T...>&>()))),
      any<T...>>;

  if (N == v.selector()) return type::template create<N>(fn(get<N>(v)));
  return impl::copy_skip<type, 0, N, sizeof...(T)>()(v);
}

template<size_t N, typename... T, typename Fn>
auto map(any<T...>&& v, Fn fn) ->
    impl::replace_type<
        N,
        decltype(_namespace(std)::declval<Fn>()(get<N>(
                     _namespace(std)::declval<any<T...>>()))),
        any<T...>> {
  using _namespace(std)::move;
  using type = impl::replace_type<
      N,
      decltype(_namespace(std)::declval<Fn>()(get<N>(
                   _namespace(std)::declval<const any<T...>&>()))),
      any<T...>>;

  if (N == v.selector()) return type::template create<N>(fn(get<N>(move(v))));
  return impl::copy_skip<type, 0, N, sizeof...(T)>()(move(v));
}


template<typename... T, typename... Fn>
auto map(any<T...>& v, Fn&&... fn) ->
    decltype(_namespace(std)::declval<impl::recursive_map<0, sizeof...(T)>>()(
                 _namespace(std)::declval<any<T...>&>(),
                 _namespace(std)::declval<Fn>()...))
{
  return impl::recursive_map<0, sizeof...(T)>()(
             v, _namespace(std)::forward<Fn>(fn)...);
}

template<typename... T, typename... Fn>
auto map(const any<T...>& v, Fn&&... fn) ->
    decltype(_namespace(std)::declval<impl::recursive_map<0, sizeof...(T)>>()(
                 _namespace(std)::declval<const any<T...>&>(),
                 _namespace(std)::declval<Fn>()...))
{
  return impl::recursive_map<0, sizeof...(T)>()(
             v, _namespace(std)::forward<Fn>(fn)...);
}

template<typename... T, typename... Fn>
auto map(any<T...>&& v, Fn&&... fn) ->
    decltype(_namespace(std)::declval<impl::recursive_map<0, sizeof...(T)>>()(
                 _namespace(std)::declval<any<T...>>(),
                 _namespace(std)::declval<Fn>()...))
{
  using _namespace(std)::move;
  return impl::recursive_map<0, sizeof...(T)>()(
             move(v), _namespace(std)::forward<Fn>(fn)...);
}


template<typename Result, typename... T, typename... Fn>
auto map_onto(any<T...>& v, Fn&&... fn) -> Result {
  return impl::recursive_map_onto<Result, 0, sizeof...(T)>()(
             v, _namespace(std)::forward<Fn>(fn)...);
}

template<typename Result, typename... T, typename... Fn>
auto map_onto(const any<T...>& v, Fn&&... fn) -> Result {
  return impl::recursive_map_onto<Result, 0, sizeof...(T)>()(
             v, _namespace(std)::forward<Fn>(fn)...);
}

template<typename Result, typename... T, typename... Fn>
auto map_onto(any<T...>&& v, Fn&&... fn) -> Result {
  using _namespace(std)::move;
  return impl::recursive_map_onto<Result, 0, sizeof...(T)>()(
             move(v), _namespace(std)::forward<Fn>(fn)...);
}


} /* namespace monsoon */

#endif /* _ILIAS_ANY_INL_H_ */
