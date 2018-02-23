#ifndef MONSOON_OBJPIPE_INTERLOCK_H
#define MONSOON_OBJPIPE_INTERLOCK_H

///\file
///\ingroup objpipe

#include <tuple>
#include <monsoon/objpipe/detail/interlock_pipe.h>
#include <monsoon/objpipe/detail/adapter.h>

namespace monsoon {
namespace objpipe {


/**
 * \brief Reader side of the interlock objpipe.
 */
template<typename T>
using interlock_reader = detail::adapter_t<detail::interlock_pipe<T>>;

/**
 * \brief Writer side of the interlock objpipe.
 */
template<typename T>
using interlock_writer = detail::interlock_writer<T>;

/**
 * \brief Create a new interlocked objpipe.
 * \ingroup objpipe
 *
 * \tparam T The type of elements used in the interlocked pipe.
 * \return A tuple pair,
 * with a \ref monsoon::objpipe::interlock_reader "reader"
 * and a \ref monsoon::objpipe::interlock_writer "writer",
 * both sharing the same interlocked object pipe.
 * \sa \ref monsoon::objpipe::detail::interlocked<T>
 */
template<typename T>
auto new_interlock()
-> std::tuple<interlock_reader<T>, interlock_writer<T>> {
  detail::interlock_impl<T>*const ptr = new detail::interlock_impl<T>();
  return std::make_tuple( // Never throws.
      interlock_reader<T>(detail::interlock_pipe<T>(ptr)),
      interlock_writer<T>(ptr));
}


}} /* namespace monsoon::objpipe */

#endif /* MONSOON_OBJPIPE_INTERLOCK_H */
