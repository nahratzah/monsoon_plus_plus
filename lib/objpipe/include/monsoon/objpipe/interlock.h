#ifndef MONSOON_OBJPIPE_INTERLOCK_H
#define MONSOON_OBJPIPE_INTERLOCK_H

///\file
///\ingroup objpipe

#include <monsoon/objpipe/detail/interlock_pipe.h>
#include <monsoon/objpipe/detail/adapter.h>

namespace monsoon {
namespace objpipe {


/**
 * \brief Create a new interlocked objpipe.
 * \ingroup objpipe
 *
 * \tparam T The type of elements used in the interlocked pipe.
 * \return A tuple pair, with a reader and a writer, both
 * sharing the same interlocked object pipe.
 * \sa \ref monsoon::objpipe::detail::interlocked<T>
 */
template<typename T>
auto new_interlock()
-> std::tuple<detail::adapter_t<detail::interlock_pipe<T>>, detail::interlock_writer<T>> {
  using reader_type = detail::adapter_t<detail::interlock_pipe<T>>;
  using writer_type = detail::interlock_writer<T>;

  detail::interlock_pipe<T>*const ptr = new detail::interlock_impl<T>();
  return std::make_tuple( // Never throws.
      reader_type<T>(detail::interlock_pipe<T>(ptr)),
      writer_type<T>(ptr));
}


}} /* namespace monsoon::objpipe */

#endif /* MONSOON_OBJPIPE_INTERLOCK_H */
