#ifndef MONSOON_IO_PTR_STREAM_INL_H
#define MONSOON_IO_PTR_STREAM_INL_H

#include <utility>

namespace monsoon {
namespace io {


template<typename Reader, typename... Args>
ptr_stream_reader make_ptr_reader(Args&&... args) {
  return ptr_stream_reader(
      std::make_unique<Reader>(std::forward<Args>(args)...));
}

template<typename Writer, typename... Args>
ptr_stream_writer make_ptr_writer(Args&&... args) {
  return ptr_stream_writer(
      std::make_unique<Writer>(std::forward<Args>(args)...));
}


}} /* namespace monsoon::io */

#endif /* MONSOON_IO_PTR_STREAM_INL_H */
