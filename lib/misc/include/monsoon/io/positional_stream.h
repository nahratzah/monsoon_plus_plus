#ifndef MONSOON_POSITIONAL_STREAM_H
#define MONSOON_POSITIONAL_STREAM_H

#include <monsoon/misc_export_.h>
#include <monsoon/io/fd.h>
#include <monsoon/io/stream.h>

namespace monsoon {
namespace io {


class monsoon_misc_export_ positional_reader
: public stream_reader
{
 public:
  positional_reader() = default;
  positional_reader(const positional_reader&) = default;
  positional_reader(const fd& fd, fd::offset_type = 0) noexcept;
  positional_reader& operator=(const positional_reader&) = default;

  ~positional_reader() noexcept override;

  std::size_t read(void*, std::size_t) override;
  void close() override;
  bool at_end() const override;

 private:
  const fd* fd_ = nullptr;
  fd::offset_type off_ = 0;
};

class monsoon_misc_export_ positional_writer
: public stream_writer
{
 public:
  positional_writer() = default;
  positional_writer(const positional_writer&) = default;
  positional_writer(fd& fd, fd::offset_type = 0) noexcept;
  positional_writer& operator=(const positional_writer&) = default;

  ~positional_writer() noexcept override;

  std::size_t write(const void*, std::size_t) override;
  void close() override;
  fd::offset_type offset() const noexcept { return off_; }

 private:
  fd* fd_ = nullptr;
  fd::offset_type off_ = 0;
};


}} /* namespace monsoon::io */

#include "positional_stream-inl.h"

#endif /* MONSOON_POSITIONAL_STREAM_H */
