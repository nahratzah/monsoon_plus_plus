#ifndef RAW_FILE_SEGMENT_WRITER_H
#define RAW_FILE_SEGMENT_WRITER_H

#include <cstddef>
#include <monsoon/io/fd.h>
#include <monsoon/io/positional_stream.h>
#include <monsoon/history/dir/dirhistory_export_.h>
#include <boost/crc.hpp>

namespace monsoon {
namespace history {


class monsoon_dirhistory_local_ raw_file_segment_writer
: public io::positional_writer
{
 public:
  raw_file_segment_writer() = default;
  raw_file_segment_writer(const raw_file_segment_writer&) = default;
  raw_file_segment_writer& operator=(const raw_file_segment_writer&) = default;
  raw_file_segment_writer(io::fd&, io::fd::offset_type,
      io::fd::size_type* = nullptr, io::fd::size_type* = nullptr);
  ~raw_file_segment_writer() noexcept override;

  std::size_t write(const void* buf, std::size_t len) override;
  void close() override;

 private:
  std::size_t write_padding_();
  std::size_t write_crc_();

  io::fd::size_type wlen_ = 0;
  boost::crc_32_type crc32_;
  io::fd::size_type *out_data_len_ = nullptr;
  io::fd::size_type *out_storage_len_ = nullptr;
};


}} /* namespace monsoon::history */

#endif /* RAW_FILE_SEGMENT_WRITER_H */
