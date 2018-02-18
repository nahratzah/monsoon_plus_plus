#ifndef MONSOON_OBJPIPE_OF_H
#define MONSOON_OBJPIPE_OF_H

///\file
///\ingroup objpipe

#include <array>
#include <type_traits>
#include <monsooon/objpipe/detail/of.h>
#include <monsooon/objpipe/detail/adapter.h>

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
noexcept(std::is_nothrow_constructible<of_pipe<std::remove_cv_t<std::remove_reference_t<T>>>, T>)
-> adapter<of_pipe<std::remove_cv_t<std::remove_reference_t<T>>>> {
  return of_pipe<std::remove_cv_t<std::remove_reference_t<T>>>(std::forward<T>(v));
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
noexcept(noexcept(of(std::make_array<T>(std::forward<Types>(values)...))
        .flatten()))
-> decltype(auto) {
  return of(std::make_array<T>(std::forward<Types>(values)...))
      .flatten();
}


} /* namespace monsoon::objpipe */

#endif /* MONSOON_OBJPIPE_OF_H */
