#ifndef MONSOON_POSITIONAL_STREAM_H
#define MONSOON_POSITIONAL_STREAM_H

#include <monsoon/fd.h>
#include <monsoon/stream.h>

namespace monsoon {


class positional_reader
: public stream_reader
{
 public:
  positional_reader() = default;
  positional_reader(const positional_reader&) = default;
  positional_reader(fd& fd, fd::offset_type = 0) noexcept;
  positional_reader& operator=(const positional_reader&) = default;

  ~positional_reader() noexcept override;

  std::size_t read(void*, std::size_t) override;

 private:
  fd* fd_ = nullptr;
  fd::offset_type off_ = 0;
};

class positional_writer
: public stream_writer
{
 public:
  positional_writer() = default;
  positional_writer(const positional_writer&) = default;
  positional_writer(fd& fd, fd::offset_type = 0) noexcept;
  positional_writer& operator=(const positional_writer&) = default;

  ~positional_writer() noexcept override;

  std::size_t write(const void*, std::size_t) override;

 private:
  fd* fd_ = nullptr;
  fd::offset_type off_ = 0;
};


} /* namespace monsoon */

#include "positional_stream-inl.h"

#endif /* MONSOON_POSITIONAL_STREAM_H */
