#ifndef MONSOON_IO_PTR_STREAM_INL_H
#define MONSOON_IO_PTR_STREAM_INL_H

#include <utility>

namespace monsoon {
namespace io {


inline const std::unique_ptr<stream_reader>& ptr_stream_reader::get() const & {
  return nested_;
}

inline std::unique_ptr<stream_reader>& ptr_stream_reader::get() & {
  return nested_;
}

inline std::unique_ptr<stream_reader>&& ptr_stream_reader::get() && {
  return std::move(nested_);
}


inline const std::unique_ptr<stream_writer>& ptr_stream_writer::get() const & {
  return nested_;
}

inline std::unique_ptr<stream_writer>& ptr_stream_writer::get() & {
  return nested_;
}

inline std::unique_ptr<stream_writer>&& ptr_stream_writer::get() && {
  return std::move(nested_);
}


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
