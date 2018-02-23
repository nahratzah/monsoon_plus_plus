#ifndef MONSOON_OBJPIPE_CALLBACK_H
#define MONSOON_OBJPIPE_CALLBACK_H

///\file monsoon/objpipe/callback.h <monsoon/objpipe/callback.h>
///\ingroup objpipe

#include <utility>
#include <monsoon/objpipe/detail/adapter.h>
#include <monsoon/objpipe/detail/callback_pipe.h>

namespace monsoon {
namespace objpipe {


/**
 * \brief Create a new callbacked objpipe.
 * \ingroup objpipe
 *
 * \tparam T The type of elements used in the callbacked pipe.
 * If T is a const type, the callback will operate on const references.
 * \tparam Fn The type of the functor, that is to be invoked with a suitable callback.
 * \param[in] fn The functor that is to be invoked.
 * \return A reader that yields each element supplied by the callback.
 * \sa \ref monsoon::objpipe::detail::callback_pipe<T>
 */
template<typename T, typename Fn>
auto new_callback(Fn&& fn)
noexcept(noexcept(detail::adapter(detail::callback_pipe<T, std::decay_t<Fn>>(std::forward<Fn>(fn)))))
-> decltype(auto) {
  return detail::adapter(detail::callback_pipe<T, std::decay_t<Fn>>(std::forward<Fn>(fn)));
}


}} /* namespace monsoon::objpipe */

#endif /* MONSOON_OBJPIPE_CALLBACK_H */
