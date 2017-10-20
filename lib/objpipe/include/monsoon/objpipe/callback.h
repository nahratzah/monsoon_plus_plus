#ifndef MONSOON_OBJPIPE_CALLBACK_H
#define MONSOON_OBJPIPE_CALLBACK_H

///@file monsoon/objpipe/callback.h <monsoon/objpipe/callback.h>

#include <monsoon/objpipe/detail/base_objpipe.h>
#include <monsoon/objpipe/detail/callbacked.h>
#include <monsoon/objpipe/reader.h>

namespace monsoon {
namespace objpipe {


/**
 * @brief Create a new callbacked objpipe.
 *
 * @tparam T The type of elements used in the callbacked pipe.
 * @tparam Fn The type of the functor, that is to be invoked with a suitable callback.
 * @param fn The functor that is to be invoked.
 * @return A reader that yields each element supplied by the callback.
 * @sa @ref monsoon::objpipe::detail::callbacked<T>
 */
template<typename T, typename Fn>
auto new_callback(Fn&& fn) -> reader<T> {
  detail::callbacked<T>* ptr = new detail::callbacked<T>(std::forward<Fn>(fn));
  return reader<T>(detail::reader_release::link(ptr)); // Never throws
}


}} /* namespace monsoon::objpipe */

#endif /* MONSOON_OBJPIPE_CALLBACK_H */
