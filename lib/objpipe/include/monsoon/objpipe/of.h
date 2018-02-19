#ifndef MONSOON_OBJPIPE_OF_H
#define MONSOON_OBJPIPE_OF_H

///\file
///\ingroup objpipe

#include <array>
#include <type_traits>
#include <monsoon/objpipe/detail/of_pipe.h>
#include <monsoon/objpipe/detail/adapter.h>

namespace monsoon::objpipe {


/**
 * \brief Create an objpipe containing a single value.
 * \ingroup objpipe
 *
 * \details
 * The value is copied during construction of the objpipe.
 *
 * If the argument is an std::reference_wrapper, a reference will be used instead.
 * In this case, the lifetime of the referenced value must exceed the lifetime of the objpipe.
 *
 * \param[in] v The value to iterate over.
 * \return An objpipe iterating over the single argument value.
 * \sa \ref monsoon::objpipe::detail::of_pipe
 */
template<typename T>
constexpr auto of(T&& v)
noexcept(std::is_nothrow_constructible_v<detail::of_pipe<std::remove_cv_t<std::remove_reference_t<T>>>, T>)
-> detail::adapter_t<detail::of_pipe<std::remove_cv_t<std::remove_reference_t<T>>>> {
  return detail::adapter(detail::of_pipe<std::remove_cv_t<std::remove_reference_t<T>>>(std::forward<T>(v)));
}

/**
 * \brief Create an objpipe containing a sequence of values.
 * \ingroup objpipe
 *
 * \details
 * The values are copied during construction of the objpipe.
 *
 * \param[in] values The values to iterate over.
 * \return An objpipe iterating over the argument values.
 * \sa \ref monsoon::objpipe::detail::of_pipe
 */
template<typename T = void, typename... Types>
constexpr auto of(Types&&... values)
noexcept(noexcept(
        of(std::array<std::conditional_t<std::is_same_v<void, T>, std::common_type_t<std::remove_cv_t<std::remove_reference_t<Types>>...>, T>>(std::forward<Types>(values...)))
        .flatten()))
-> decltype(auto) {
  using type = std::conditional_t<
      std::is_same_v<void, T>,
      std::common_type_t<std::remove_cv_t<std::remove_reference_t<Types>>...>,
      T>;

  return of(std::array<type>(std::forward<Types>(values)...))
      .flatten();
}


} /* namespace monsoon::objpipe */

#endif /* MONSOON_OBJPIPE_OF_H */
