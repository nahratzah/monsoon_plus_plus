#ifndef MONSOON_OBJPIPE_INTERLOCK_H
#define MONSOON_OBJPIPE_INTERLOCK_H

///\file
///\ingroup objpipe

#include <monsoon/objpipe/detail/base_objpipe.h>
#include <monsoon/objpipe/detail/interlocked.h>
#include <monsoon/objpipe/reader.h>
#include <monsoon/objpipe/writer.h>

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
auto new_interlock() -> std::tuple<reader<T>, writer<T>> {
  detail::interlocked<T>* ptr = new detail::interlocked<T>();
  return std::make_tuple( // Never throws.
      reader<T>(detail::reader_release::link(ptr)),
      writer<T>(detail::writer_release::link(ptr)));
}


}} /* namespace monsoon::objpipe */

#endif /* MONSOON_OBJPIPE_INTERLOCK_H */
