#include "raw_file_segment_reader.h"
#include <monsoon/history/dir/hdir_exception.h>
#include <array>
#include <cstring>

namespace monsoon {
namespace history {


raw_file_segment_reader::raw_file_segment_reader(io::fd& file,
    io::fd::offset_type offset, io::fd::size_type length)
: io::positional_reader(file, offset),
  avail_(length),
  pad_len_(avail_ % 4u != 0 ? 4u - avail_ % 4u : 0u)
{}

raw_file_segment_reader::~raw_file_segment_reader() noexcept {}

std::size_t raw_file_segment_reader::read(void* buf, std::size_t len) {
  if (avail_ == 0) return 0;
  if (len > avail_) len = avail_;
  const auto rlen = io::positional_reader::read(buf, len);
  crc32_.process_bytes(buf, rlen);
  if (rlen == 0 && io::positional_reader::at_end())
    throw dirhistory_exception("raw_file_segment_reader end-of-stream");
  avail_ -= rlen;
  return rlen;
}

void raw_file_segment_reader::close() {
  if (avail_ != 0)
    throw dirhistory_exception("raw_file_segment_reader data remaining");
  consume_padding_();
  const auto expected_crc32 = read_expected_crc32_();
  io::positional_reader::close();

  if (expected_crc32 != crc32_.checksum())
    throw dirhistory_exception("raw_file_segment_reader CRC mismatch");
}

bool raw_file_segment_reader::at_end() const {
  return avail_ == 0;
}

void raw_file_segment_reader::consume_padding_() {
  std::array<uint8_t, 4> buf;
  while (pad_len_ > 0) {
    const auto rlen = io::positional_reader::read(buf.data(), std::min(buf.size(), pad_len_));
    crc32_.process_bytes(buf.data(), rlen);
    if (rlen == 0 && io::positional_reader::at_end())
      throw dirhistory_exception("raw_file_segment_reader failed to read padding bytes");
    if (std::any_of(buf.begin(), buf.begin() + rlen, [](std::uint8_t byte) { return byte != 0u; }))
      throw dirhistory_exception("raw_file_segment_reader non-zero bytes in padding");
    pad_len_ -= rlen;
  }
}

std::uint32_t raw_file_segment_reader::read_expected_crc32_() {
  std::array<std::uint8_t, 4> buf;

  {
    std::size_t len = buf.size();
    std::uint8_t* buf_ptr = buf.data();
    while (len > 0u) {
      const auto rlen = io::positional_reader::read(buf_ptr, len);
      if (rlen == 0 && io::positional_reader::at_end())
        throw dirhistory_exception("raw_file_segment_reader unable to read CRC");
      len -= rlen;
      buf_ptr += rlen;
    }
  }

  std::uint32_t i;
  static_assert(sizeof(i) == buf.size(), "Buffer size mismatch");
  std::memcpy(&i, buf.data(), sizeof(i)); // memcpy is blessed not to cause type punning issues.
  return boost::endian::big_to_native(i);
}


}} /* namespace monsoon::history */
