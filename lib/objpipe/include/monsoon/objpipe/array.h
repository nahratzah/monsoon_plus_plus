#ifndef MONSOON_OBJPIPE_ARRAY_H
#define MONSOON_OBJPIPE_ARRAY_H

///\file
///\ingroup objpipe

#include <monsoon/objpipe/detail/array_pipe.h>

namespace monsoon {
namespace objpipe {


/**
 * \brief Create an objpipe that iterates over the given range.
 * \ingroup objpipe
 *
 * \details The elements in the range are copied at construction time.
 *
 * \note If you want to iterate an entire collection,
 * \ref monsoon::objpipe::of "objpipe::of"
 * followed by
 * \ref monsoon::ojbpipe::detail::adapter_t::flatten ".flatten()"
 * will have better performance.
 * \code
 * of(collection).flatten()
 * \endcode
 *
 * \param[in] b,e The values to iterate over.
 * \param[in] alloc The allocator used to allocate internal storage.
 * \return An objpipe that iterates the values in the given range.
 * \sa \ref monsoon::ojbpipe::detail::array_pipe
 */
template<typename Iter, typename Alloc = std::allocator<typename std::iterator_traits<Iter>::value_type>>
auto new_array(Iter b, Iter e, Alloc alloc = Alloc())
-> array_pipe<typename std::iterator_traits<Iter>::value_type, Alloc> {
  return array_pipe<typename std::iterator_traits<Iter>::value_type, Alloc>(b, e, alloc);
}

/**
 * \brief Create an objpipe that iterates over the given values.
 * \ingroup objpipe
 *
 * \details The elements in the range are copied at construction time.
 *
 * \note If you want to iterate an entire collection,
 * \ref monsoon::objpipe::of "objpipe::of"
 * followed by
 * \ref monsoon::ojbpipe::detail::adapter_t::flatten ".flatten()"
 * will have better performance.
 * \code
 * of(collection).flatten()
 * \endcode
 *
 * \param[in] values The values to iterate over.
 * \param[in] alloc The allocator used to allocate internal storage.
 * \return An objpipe that iterates the values in the given range.
 * \sa \ref monsoon::ojbpipe::detail::array_pipe
 */
template<typename T, typename Alloc = std::allocator<T>>
auto new_array(std::initializer_list<T> values, Alloc alloc = Alloc())
-> array_pipe<T, Alloc> {
  return array_pipe<T, Alloc>(values, alloc);
}


}} /* namespace monsoon::objpipe */

#endif /* MONSOON_OBJPIPE_ARRAY_H */
