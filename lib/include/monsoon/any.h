#ifndef _ILIAS_ANY_H_
#define _ILIAS_ANY_H_

#include <monsoon/optional.h>
#include <iosfwd>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

namespace monsoon {


template<typename...> class any;


namespace impl {


using namespace ::std;

template<typename T> using any_decorate =
    conditional_t<is_lvalue_reference<T>::value,
                  add_pointer_t<T>,
                  remove_reference_t<T>>;

template<size_t N, typename T0, typename... T>
struct select_n {
  static_assert(N <= sizeof...(T),
                "Index too high.");

  using type = typename select_n<N - 1, T...>::type;
};

template<typename T0, typename... T>
struct select_n<0U, T0, T...> {
  using type = T0;
};

template<size_t N, typename... T> using select_n_t =
    typename select_n<N, T...>::type;

template<typename Sel, typename T0, typename... T>
struct type_to_index_ {
  static constexpr size_t value = (is_same<Sel, T0>::value ?
                                   0U :
                                   1U + type_to_index_<Sel, T...>::value);
};
template<typename Sel, typename... T> using type_to_index =
    integral_constant<size_t, type_to_index_<Sel, T...>::value>;


template<typename... T> struct type_list;

template<typename, typename> struct concat_type_list_;  // Unimplemented
template<typename... T, typename... U>
struct concat_type_list_<type_list<T...>, type_list<U...>> {
  using type = type_list<T..., U...>;
};
template<typename T, typename U> using concat_type_list =
    typename concat_type_list_<T, U>::type;

template<size_t, typename> struct split_type_list;  // Unimplemented
template<size_t N, typename T0, typename... T>
struct split_type_list<N, type_list<T0, T...>> {
  using left = concat_type_list<
                   type_list<T0>,
                   typename split_type_list<N - 1, type_list<T...>>::left>;
  using right = typename split_type_list<N - 1, type_list<T...>>::right;
};
template<>
struct split_type_list<0, type_list<>> {
  using left = type_list<>;
  using right = type_list<>;
};
template<typename T0, typename... T>
struct split_type_list<0, type_list<T0, T...>> {
  using left = type_list<>;
  using right = type_list<T0, T...>;
};

template<typename> struct type_list_to_any_;  // Unimplemented
template<typename T> using type_list_to_any =
    typename type_list_to_any_<T>::type;
template<typename... T>
struct type_list_to_any_<type_list<T...>> {
  using type = any<T...>;
};

template<size_t, typename, typename> struct replace_type_;  // Unimplemented
template<size_t N, typename U, typename T> using replace_type =
    typename replace_type_<N, U, T>::type;
/* Replace type at index. */
template<size_t N, typename U, typename... T>
struct replace_type_<N, U, any<T...>> {
 private:
  using left = typename split_type_list<N, type_list<T...>>::left;
  using mid = type_list<U>;
  using right = typename split_type_list<N + 1, type_list<T...>>::right;

 public:
  using type =
      type_list_to_any<concat_type_list<concat_type_list<left, mid>, right>>;
};


template<typename...> struct is_nothrow_move_constructible_any_;
template<typename... T> using is_nothrow_move_constructible_any =
    integral_constant<bool, is_nothrow_move_constructible_any_<T...>::value>;

template<typename...> struct is_nothrow_copy_constructible_any_;
template<typename... T> using is_nothrow_copy_constructible_any =
    integral_constant<bool, is_nothrow_copy_constructible_any_<T...>::value>;

template<typename...> struct is_nothrow_move_assignable_any_;
template<typename... T> using is_nothrow_move_assignable_any =
    integral_constant<bool, is_nothrow_move_assignable_any_<T...>::value>;

template<typename...> struct is_nothrow_copy_assignable_any_;
template<typename... T> using is_nothrow_copy_assignable_any =
    integral_constant<bool, is_nothrow_copy_assignable_any_<T...>::value>;

template<typename, typename...> struct any_constructor_resolution;

template<size_t, size_t> struct recursive_map;

template<size_t Idx, size_t End, typename... T> struct copy_operation;

template<size_t N, typename... T>
struct eq_ {
  bool operator()(const any<T...>&, const any<T...>&) const noexcept;
};
template<typename... T>
struct eq_<0, T...> {
  bool operator()(const any<T...>&, const any<T...>&) const noexcept;
};


} /* namespace monsoon::impl */


class any_error
: public std::runtime_error
{
 public:
  explicit any_error(const std::string&);
  explicit any_error(const char*);

  ~any_error() noexcept override;

  static void __throw()
      __attribute__((__noreturn__));
  static void __throw(const std::string&)
      __attribute__((__noreturn__));
  static void __throw(const char*)
      __attribute__((__noreturn__));
};


template<size_t N, typename... T>
auto get(any<T...>&) ->
    std::add_lvalue_reference_t<impl::select_n_t<N, T...>>;

template<size_t N, typename... T>
auto get(const any<T...>&) ->
    std::add_lvalue_reference_t<std::add_const_t<
        impl::select_n_t<N, T...>>>;

template<size_t N, typename... T>
auto get(any<T...>&&) ->
    std::add_rvalue_reference_t<impl::select_n_t<N, T...>>;


template<typename Sel, typename... T>
auto get(any<T...>&) ->
    decltype(get<impl::type_to_index<Sel, T...>::value>(
                 std::declval<any<T...>&>()));

template<typename Sel, typename... T>
auto get(const any<T...>&) ->
    decltype(get<impl::type_to_index<Sel, T...>::value>(
                 std::declval<const any<T...>&>()));

template<typename Sel, typename... T>
auto get(any<T...>&&) ->
    decltype(get<impl::type_to_index<Sel, T...>::value>(
                 std::declval<any<T...>>()));


template<typename... T>
class any {
  template<size_t N, typename... U>
  friend auto ::monsoon::get(any<U...>&) ->
      std::add_lvalue_reference_t<impl::select_n_t<N, U...>>;
  template<size_t N, typename... U>
  friend auto ::monsoon::get(const any<U...>&) ->
      std::add_lvalue_reference_t<std::add_const_t<
          impl::select_n_t<N, U...>>>;
  template<size_t N, typename... U>
  friend auto ::monsoon::get(any<U...>&&) ->
      std::add_rvalue_reference_t<impl::select_n_t<N, U...>>;

  template<size_t, size_t, typename...>
  friend struct ::monsoon::impl::copy_operation;

 private:
  using storage_t = std::aligned_union_t<
      0, typename impl::any_decorate<T>...>;

  constexpr any() noexcept;

 public:
  any(const any&) noexcept(impl::is_nothrow_copy_constructible_any<T...>());
  any(any&&) noexcept(impl::is_nothrow_move_constructible_any<T...>());
  ~any() noexcept;

  any& operator=(const any&)
      noexcept(impl::is_nothrow_copy_assignable_any<T...>());
  any& operator=(any&&) noexcept(impl::is_nothrow_move_assignable_any<T...>());

  template<typename U,
           size_t N = std::enable_if_t<
               impl::any_constructor_resolution<U, T...>::found,
               impl::any_constructor_resolution<U, T...>>::idx>
      any(U&& v);

  size_t selector() const noexcept { return std::get<0>(data_); }

  template<size_t N> static auto create(impl::select_n_t<N, T...>) ->
      std::enable_if_t<std::is_lvalue_reference<
                                       impl::select_n_t<N, T...>>::value,
                                   any>;
  template<size_t N> static auto create(impl::select_n_t<N, T...>) ->
      std::enable_if_t<!std::is_lvalue_reference<
                                        impl::select_n_t<N, T...>>::value,
                                   any>;

  bool operator==(const any&) const noexcept;
  bool operator!=(const any&) const noexcept;

 private:
  std::tuple<
      std::size_t, storage_t> data_;
};


template<size_t N, typename... T>
auto get_optional(const any<T...>&) ->
    optional<impl::select_n_t<N, T...>>;

template<size_t N, typename... T>
auto get_optional(any<T...>&&) ->
    optional<impl::select_n_t<N, T...>>;


template<size_t N, typename... T, typename Fn>
bool visit(any<T...>&, Fn);

template<size_t N, typename... T, typename Fn>
bool visit(const any<T...>&, Fn);

template<size_t N, typename... T, typename Fn>
bool visit(any<T...>&&, Fn);


template<typename... T, typename... Fn>
auto visit(any<T...>&, Fn&&...) ->
    std::enable_if_t<sizeof...(T) == sizeof...(Fn), void>;

template<typename... T, typename... Fn>
auto visit(const any<T...>&, Fn&&...) ->
    std::enable_if_t<sizeof...(T) == sizeof...(Fn), void>;

template<typename... T, typename... Fn>
auto visit(any<T...>&&, Fn&&...) ->
    std::enable_if_t<sizeof...(T) == sizeof...(Fn), void>;


template<size_t N, typename... T, typename Fn>
auto map(const any<T...>& v, Fn fn) ->
    impl::replace_type<
        N,
        decltype(std::declval<Fn>()(
                     get<N>(std::declval<const any<T...>&>()))),
        any<T...>>;

template<size_t N, typename... T, typename Fn>
auto map(any<T...>&& v, Fn fn) ->
    impl::replace_type<
        N,
        decltype(std::declval<Fn>()(
                     get<N>(std::declval<any<T...>>()))),
        any<T...>>;


template<typename... T, typename... Fn>
auto map(any<T...>&, Fn&&...) ->
    decltype(std::declval<impl::recursive_map<0, sizeof...(T)>>()(
                 std::declval<any<T...>&>(),
                 std::declval<Fn>()...));

template<typename... T, typename... Fn>
auto map(const any<T...>&, Fn&&...) ->
    decltype(std::declval<impl::recursive_map<0, sizeof...(T)>>()(
                 std::declval<const any<T...>&>(),
                 std::declval<Fn>()...));

template<typename... T, typename... Fn>
auto map(any<T...>&&, Fn&&...) ->
    decltype(std::declval<impl::recursive_map<0, sizeof...(T)>>()(
                 std::declval<any<T...>>(),
                 std::declval<Fn>()...));


} /* namespace monsoon */

#include "any-inl.h"

#endif /* _ILIAS_ANY_H_ */
