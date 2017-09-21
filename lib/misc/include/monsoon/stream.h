#ifndef MONSOON_STREAM_H
#define MONSOON_STREAM_H

#include <cstdlib>

namespace monsoon {


class stream_reader {
 public:
  virtual ~stream_reader() noexcept;
  virtual std::size_t read(void*, std::size_t) = 0;
};

class stream_writer {
 public:
  virtual ~stream_writer() noexcept;
  virtual std::size_t write(const void*, std::size_t) = 0;
  virtual void close() = 0;
};


} /* namespace monsoon */

#endif /* MONSOON_STREAM_H */
