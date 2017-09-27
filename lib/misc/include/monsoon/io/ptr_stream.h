#ifndef MONSOON_IO_PTR_STREAM_H
#define MONSOON_IO_PTR_STREAM_H

#include <monsoon/misc_export_.h>
#include <monsoon/io/stream.h>
#include <memory>

namespace monsoon {
namespace io {


class monsoon_misc_export_ ptr_stream_reader
: public stream_reader
{
 public:
  ptr_stream_reader() = default;
  ptr_stream_reader(ptr_stream_reader&&) noexcept;
  ptr_stream_reader& operator=(ptr_stream_reader&&) noexcept;
  ptr_stream_reader(std::unique_ptr<stream_reader>&&) noexcept;
  ~ptr_stream_reader() noexcept;

  std::size_t read(void*, std::size_t) override;
  void close() override;
  bool at_end() const override;

 private:
  std::unique_ptr<stream_reader> nested_;
};

class ptr_stream_writer
: public stream_writer
{
 public:
  ptr_stream_writer() = default;
  ptr_stream_writer(ptr_stream_writer&&) noexcept;
  ptr_stream_writer& operator=(ptr_stream_writer&&) noexcept;
  ptr_stream_writer(std::unique_ptr<stream_writer>&&) noexcept;
  ~ptr_stream_writer() noexcept;

  std::size_t write(const void*, std::size_t) override;
  void close() override;

 private:
  std::unique_ptr<stream_writer> nested_;
};


}} /* namespace monsoon::io */

#endif /* MONSOON_IO_PTR_STREAM_H */
