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

  const std::unique_ptr<stream_reader>& get() const &;
  std::unique_ptr<stream_reader>& get() &;
  std::unique_ptr<stream_reader>&& get() &&;

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

  const std::unique_ptr<stream_writer>& get() const &;
  std::unique_ptr<stream_writer>& get() &;
  std::unique_ptr<stream_writer>&& get() &&;

 private:
  std::unique_ptr<stream_writer> nested_;
};

template<typename, typename... Args>
ptr_stream_reader make_ptr_reader(Args&&... args);

template<typename, typename... Args>
ptr_stream_writer make_ptr_writer(Args&&... args);


}} /* namespace monsoon::io */

#include "ptr_stream-inl.h"

#endif /* MONSOON_IO_PTR_STREAM_H */
