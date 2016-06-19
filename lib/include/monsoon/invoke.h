#ifndef MONSOON_INVOKE_H
#define MONSOON_INVOKE_H

#include <type_traits>
#include <utility>

namespace monsoon {


/*
 * INVOKE:  (t1.*f)(t2, ..., tN)
 * when f is a pointer to member function of a class T and
 * - t1 is an object of type T or
 * - t1 is a reference to an object of type T or
 * - t1 is a reference to an object derived from T.
 */
template<typename F, typename T, typename... Args, typename T1, typename... TT>
auto invoke(F (T::*f)(Args...), T1&& t1, TT&&... tt)
    noexcept(noexcept((std::forward<T1>(t1).*f)(std::forward<TT>(tt)...))) ->
    std::enable_if_t<(std::is_same<T, std::remove_cv_t<std::remove_reference_t<T1>>>::value ||
                      std::is_base_of<T, std::remove_cv_t<std::remove_reference_t<T1>>>::value),
                     decltype((std::forward<T1>(t1).*f)(std::forward<TT>(tt)...))> {
  return (std::forward<T1>(t1).*f)(std::forward<TT>(tt)...);
}

/*
 * INVOKE:  ((*t1).*f)(t2, ..., tN)
 * when f is a pointer to member function of a class T and
 * t1 is not one of the types described in the previous item.
 */
template<typename F, typename T, typename... Args, typename T1, typename... TT>
auto invoke(F (T::*f)(Args...), T1&& t1, TT&&... tt)
    noexcept(noexcept(((*std::forward<T1>(t1)).*f)(std::forward<TT>(tt)...))) ->
    std::enable_if_t<!(std::is_same<T, std::remove_cv_t<std::remove_reference_t<T1>>>::value ||
                       std::is_base_of<T, std::remove_cv_t<std::remove_reference_t<T1>>>::value),
                     decltype(((*std::forward<T1>(t1)).*f)(std::forward<TT>(tt)...))> {
  return ((*std::forward<T1>(t1)).*f)(std::forward<TT>(tt)...);
}

/*
 * INVOKE:  t1.*f
 * when N == 1 and f is a pointer to member data of a class T and
 * - t1 is an object of type T or
 * - t1 is a reference to an object of type T or
 * - t1 is a reference to an object derived from T.
 */
template<typename F, typename T, typename T1>
auto invoke(F T::*f, T1&& t1)
    noexcept(noexcept(std::forward<T1>(t1).*f)) ->
    std::enable_if_t<(std::is_same<T, std::remove_cv_t<std::remove_reference_t<T1>>>::value ||
                      std::is_base_of<T, std::remove_cv_t<std::remove_reference_t<T1>>>::value),
                     decltype(std::forward<T1>(t1).*f)> {
  return std::forward<T1>(t1).*f;
}

/*
 * INVOKE:  (*t1).*f
 * when N == 1 and f is a pointer to member data of a class T and
 * t1 is not one of the types described in the previous item.
 */
template<typename F, typename T, typename T1>
auto invoke(F T::*f, T1&& t1)
    noexcept(noexcept((*std::forward<T1>(t1)).*f)) ->
    std::enable_if_t<!(std::is_same<T, std::remove_cv_t<std::remove_reference_t<T1>>>::value ||
                       std::is_base_of<T, std::remove_cv_t<std::remove_reference_t<T1>>>::value),
                     decltype((*std::forward<T1>(t1)).*f)> {
  return (*std::forward<T1>(t1)).*f;
}

/*
 * INVOKE:  f(t1, t2, ..., tN)
 * in all other cases.
 */
template<typename F, typename... Args>
auto invoke(F&& f, Args&&... args)
    noexcept(noexcept(f(std::forward<Args>(args)...))) ->
    decltype(f(std::forward<Args>(args)...)) {
  return f(std::forward<Args>(args)...);
}


} /* namespace monsoon */

#endif /* MONSOON_INVOKE_H */
