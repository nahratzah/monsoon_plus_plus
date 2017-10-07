#include "raw_file_segment_writer.h"
#include <monsoon/history/dir/hdir_exception.h>
#include <array>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <boost/endian/conversion.hpp>

namespace monsoon {
namespace history {


raw_file_segment_writer::raw_file_segment_writer(io::fd& file,
    io::fd::offset_type offset,
    io::fd::size_type* out_data_len, io::fd::size_type* out_storage_len)
: io::positional_writer(file, offset),
  out_data_len_(out_data_len),
  out_storage_len_(out_storage_len)
{}

raw_file_segment_writer::~raw_file_segment_writer() noexcept {}

std::size_t raw_file_segment_writer::write(const void* buf, std::size_t len) {
  const auto wlen = io::positional_writer::write(buf, len);
  crc32_.process_bytes(buf, wlen);
  wlen_ += wlen;
  return wlen;
}

void raw_file_segment_writer::close() {
  const auto padlen = write_padding_();
  const auto crclen = write_crc_();
  io::positional_writer::close();

  if (out_data_len_ != nullptr) *out_data_len_ = wlen_;
  if (out_storage_len_ != nullptr) *out_storage_len_ = wlen_ + padlen + crclen;
}

std::size_t raw_file_segment_writer::write_padding_() {
  std::array<std::uint8_t, 4> buf;
  std::fill(buf.begin(), buf.end(), 0u);

  if (wlen_ % 4u == 0u) return 0u;
  const std::size_t padlen = 4u - wlen_ % 4u;
  crc32_.process_bytes(buf.data(), padlen);

  const std::uint8_t* p = buf.data();
  std::size_t n = padlen;
  while (n > 0) {
    const auto wlen = io::positional_writer::write(p, n);
    assert(wlen <= n);
    n -= wlen;
    p += wlen;
  }

  return padlen;
}

std::size_t raw_file_segment_writer::write_crc_() {
  const std::uint32_t i = boost::endian::native_to_big(crc32_.checksum());

  const std::uint8_t* p = reinterpret_cast<const std::uint8_t*>(&i);
  std::size_t n = sizeof(i);
  while (n > 0) {
    const auto wlen = io::positional_writer::write(p, n);
    assert(wlen <= n);
    n -= wlen;
    p += wlen;
  }

  return sizeof(i);
}


}} /* namespace monsoon::history */
