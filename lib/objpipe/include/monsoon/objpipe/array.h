#ifndef MONSOON_OBJPIPE_ARRAY_H
#define MONSOON_OBJPIPE_ARRAY_H

///\file
///\ingroup objpipe

#include <monsoon/objpipe/detail/base_objpipe.h>
#include <monsoon/objpipe/detail/arrayed.h>
#include <monsoon/objpipe/reader.h>

namespace monsoon {
namespace objpipe {


/**
 * \brief Create a new objpipe returning a collection.
 * \ingroup objpipe
 *
 * \tparam T The type of elements used in the callbacked pipe.
 * \tparam Alloc Allocator type to store objects.
 * \param b,e Iterator range of elements that are to be emitted.
 * \param alloc Allocator to store objects.
 * \return A reader that yields each element supplied by the iterator range.
 * \note The elements are copied.
 * \sa \ref monsoon::objpipe::detail::arrayed<T>
 */
template<typename T, typename Alloc = std::allocator<T>, typename Iter>
auto new_array(Iter b, Iter e, Alloc alloc = Alloc()) -> reader<T> {
  detail::arrayed<T, std::decay_t<Alloc>>* ptr =
      new detail::arrayed<T, Alloc>(b, e, std::move(alloc));
  return reader<T>(detail::reader_release::link(ptr)); // Never throws
}

/**
 * \brief Create a new objpipe returning a collection.
 * \ingroup objpipe
 *
 * \tparam T The type of elements used in the callbacked pipe.
 * \tparam Alloc Allocator type to store objects.
 * \param il Initializer list of values to be emitted.
 * \param alloc Allocator to store objects.
 * \return A reader that yields each element supplied by the iterator range.
 * \note The elements are copied.
 * \sa \ref monsoon::objpipe::detail::arrayed<T>
 */
template<typename T, typename Alloc = std::allocator<T>>
auto new_array(std::initializer_list<T> il, Alloc alloc = Alloc())
-> reader<T> {
  detail::arrayed<T, std::decay_t<Alloc>>* ptr =
      new detail::arrayed<T, Alloc>(il, std::move(alloc));
  return reader<T>(detail::reader_release::link(ptr)); // Never throws
}


}} /* namespace monsoon::objpipe */

#endif /* MONSOON_OBJPIPE_ARRAY_H */
