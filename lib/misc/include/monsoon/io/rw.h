#ifndef RW_H
#define RW_H

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <monsoon/io/fd.h>

namespace monsoon::io {


template<typename Reader>
inline void read(Reader& reader, void* buf, std::size_t len) {
  std::uint8_t* cbuf = reinterpret_cast<std::uint8_t*>(buf);

  while (len > 0) {
    const auto rlen = reader.read(cbuf, len);
    if (rlen == 0) throw std::runtime_error("unable to read");
    cbuf += rlen;
    len -= rlen;
  }
}

template<typename Reader>
inline void read_at(Reader& reader, monsoon::io::fd::offset_type off, void* buf, std::size_t len) {
  std::uint8_t* cbuf = reinterpret_cast<std::uint8_t*>(buf);

  while (len > 0) {
    const auto rlen = reader.read_at(off, cbuf, len);
    if (rlen == 0) throw std::runtime_error("unable to read");
    off += rlen;
    cbuf += rlen;
    len -= rlen;
  }
}

template<typename Writer>
inline void write(Writer& writer, const void* buf, size_t len) {
  const std::uint8_t* cbuf = reinterpret_cast<const std::uint8_t*>(buf);

  while (len > 0) {
    const auto wlen = writer.write(cbuf, len);
    cbuf += wlen;
    len -= wlen;
  }
}

template<typename Writer>
inline void write_at(Writer& writer, monsoon::io::fd::offset_type off, const void* buf, size_t len) {
  const std::uint8_t* cbuf = reinterpret_cast<const std::uint8_t*>(buf);

  while (len > 0) {
    const auto wlen = writer.write_at(off, cbuf, len);
    off += wlen;
    cbuf += wlen;
    len -= wlen;
  }
}


} /* namespace monsoon::io */

#endif /* RW_H */
