#ifndef RAW_FILE_SEGMENT_READER_H
#define RAW_FILE_SEGMENT_READER_H

#include <cstddef>
#include <cstdint>
#include <monsoon/io/fd.h>
#include <monsoon/io/positional_stream.h>
#include <monsoon/history/dir/dirhistory_export_.h>
#include <boost/endian/conversion.hpp>
#include <boost/crc.hpp>

namespace monsoon {
namespace history {


/**
 * Raw file segment reader.
 *
 * Handles reading and validation of a file segment,
 * including correctly handling padding bytes and verifying the CRC32.
 * Does not handle any compression/decompression logic.
 *
 * The segment reader uses a reference to the file and thus
 * is only valid as long as the underlying file is valid.
 */
class monsoon_dirhistory_local_ raw_file_segment_reader
: public io::positional_reader
{
 public:
  raw_file_segment_reader() = default;
  raw_file_segment_reader(const raw_file_segment_reader&) = default;
  raw_file_segment_reader& operator=(const raw_file_segment_reader&) = default;
  raw_file_segment_reader(io::fd&, io::fd::offset_type, io::fd::size_type);
  ~raw_file_segment_reader() noexcept override;

  std::size_t read(void* buf, std::size_t len) override;
  void close() override;
  bool at_end() const override;

 private:
  void consume_padding_();
  std::uint32_t read_expected_crc32_();

  io::fd::size_type avail_, pad_len_ = 0;
  boost::crc_32_type crc32_;
};


}} /* namespace monsoon::history */

#endif /* RAW_FILE_SEGMENT_READER_H */
