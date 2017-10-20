#ifndef MONSOON_OBJPIPE_INTERLOCK_H
#define MONSOON_OBJPIPE_INTERLOCK_H

///@file monsoon/objpipe/interlock.h <monsoon/objpipe/interlock.h>

#include <monsoon/objpipe/detail/base_objpipe.h>
#include <monsoon/objpipe/detail/interlocked.h>
#include <monsoon/objpipe/reader.h>
#include <monsoon/objpipe/writer.h>

namespace monsoon {
namespace objpipe {


/**
 * @brief Create a new interlocked objpipe.
 *
 * @tparam T The type of elements used in the interlocked pipe.
 * @return A tuple pair, with a reader and a writer, both
 * sharing the same interlocked object pipe.
 * @sa @ref monsoon::objpipe::detail::interlocked<T>
 */
template<typename T>
auto new_interlock() -> std::tuple<reader<T>, writer<T>> {
  detail::interlocked* ptr = new detail::interlocked();
  return std::make_tuple( // Never throws.
      reader<T>(reader_release::link(ptr)),
      writer<T>(writer_release::link(ptr)));
}


}} /* namespace monsoon::objpipe */

#endif /* MONSOON_OBJPIPE_INTERLOCK_H */
