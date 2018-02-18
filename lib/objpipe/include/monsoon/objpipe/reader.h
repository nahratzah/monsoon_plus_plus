#ifndef MONSOON_OBJPIPE_READER_H
#define MONSOON_OBJPIPE_READER_H

///\file
///\ingroup objpipe

#include <monsoon/objpipe/detail/fwd.h>
#include <monsoon/objpipe/detail/virtual.h>

namespace monsoon {
namespace objpipe {


/**
 * \brief An object pipe reader.
 * \ingroup objpipe
 *
 * \tparam T The type of objects emitted by the object pipe.
 */
template<typename T>
class reader
: public detail::adapter_t<detail::virtual_pipe<T>>
{
 public:
  using detail::adapter_t<detail::virtual_pipe<T>>::adapter_t;
  using detail::adapter_t<detail::virtual_pipe<T>>::operator=;
};


}} /* namespace monsoon::objpipe */

#include <monsoon/objpipe/detail/adapter.h>

#endif /* MONSOON_OBJPIPE_READER_H */
