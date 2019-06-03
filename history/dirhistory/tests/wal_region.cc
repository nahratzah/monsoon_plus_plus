#include "print.h"
#include "UnitTest++/UnitTest++.h"
#include <monsoon/history/dir/io/wal.h>
#include <monsoon/io/fd.h>
#include <monsoon/io/positional_stream.h>
#include <stdexcept>
#include <iostream>

using monsoon::history::io::wal_region;

namespace {

template<typename FD>
auto set_file_contents(FD&& fd, std::vector<std::uint8_t> v) -> FD&& {
  fd.truncate(v.size());

  auto buf = v.data();
  auto len = v.size();
  monsoon::io::fd::offset_type off = 0;
  while (len > 0) {
    const auto wlen = fd.write_at(off, buf, len);
    std::cerr << "wrote " << wlen << " bytes at offset " << off << std::endl;
    buf += wlen;
    len -= wlen;
    off += wlen;
  }
  std::cerr << "file is " << fd.size() << " bytes" << std::endl;

  return std::forward<FD>(fd);
}

auto file_contents(const monsoon::io::fd& fd) -> std::vector<std::uint8_t> {
  std::vector<std::uint8_t> v;
  if (fd.size() > v.max_size()) throw std::length_error("raw file too large for vector");
  v.resize(fd.size());

  auto buf = v.data();
  auto len = v.size();
  auto r = monsoon::io::positional_reader(fd);
  while (len > 0) {
    const auto rlen = r.read(buf, len);
    buf += rlen;
    len -= rlen;
  }

  return v;
}

auto file_contents(const wal_region& wal) -> std::vector<std::uint8_t> {
  std::vector<std::uint8_t> v;
  if (wal.size() > v.max_size()) throw std::length_error("WAL file too large for vector");
  v.resize(wal.size());

  auto buf = v.data();
  auto len = v.size();
  monsoon::io::fd::offset_type off = 0;
  while (len > 0) {
    const auto rlen = wal.read_at(off, buf, len);
    buf += rlen;
    len -= rlen;
    off += rlen;
  }

  return v;
}

template<typename FD>
void check_file_equals(std::vector<std::uint8_t> expect, const FD& fd) {
  CHECK_EQUAL(expect.size(), fd.size());
  const auto fd_v = file_contents(fd);
  if (expect.size() == fd_v.size())
    CHECK_ARRAY_EQUAL(expect.data(), fd_v.data(), expect.size());
}

} /* namespace <unnamed> */

#define TMPFILE() monsoon::io::fd::tmpfile(__FILE__)
#define TMPFILE_WITH_DATA(data) set_file_contents(TMPFILE(), data)

TEST(new_file) {
  auto wal = wal_region(wal_region::create(), TMPFILE(), 0, 64);

  check_file_equals(
      {
        0u, 0u, 0u, 0u, // seqence number
        0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, // file size
        0u, 0u, 0u, 0u, // end of WAL
        // 16 bytes so far
        // zero padding rest of the segment (16 bytes)
        0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
        0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
        // Next segment
        0xffu, 0xffu, 0xffu, 0xffu, // seqence number
        0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, // file size
        0u, 0u, 0u, 0u, // end of WAL
        // zero padding rest of the segment (16 bytes)
        0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
        0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u
      },
      wal.fd());
  check_file_equals({}, wal);
}

TEST(existing_file_read_write) {
  auto wal = wal_region(
      TMPFILE_WITH_DATA(
          std::vector<std::uint8_t>({
              0u, 0u, 0u, 0u, // seqence number
              0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, // file size
              0u, 0u, 0u, 0u, // end of WAL
              // 16 bytes so far
              // padding rest of the segment (16 bytes)
              // we use non-zero padding, to check it does not get rewritten
              17u, 19u, 23u, 29u, 31u, 37u, 41u, 43u,
              17u, 19u, 23u, 29u, 31u, 37u, 41u, 43u,
              // Next segment
              0xffu, 0xffu, 0xffu, 0xffu, // seqence number
              0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, // file size
              0u, 0u, 0u, 0u, // end of WAL
              // zero padding rest of the segment (16 bytes)
              0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
              0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u
          })),
      0, 64);

  check_file_equals(
      {
        0u, 0u, 0u, 0u, // seqence number
        0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, // file size
        0u, 0u, 0u, 0u, // end of WAL
        // 16 bytes so far
        // padding rest of the segment (16 bytes)
        17u, 19u, 23u, 29u, 31u, 37u, 41u, 43u,
        17u, 19u, 23u, 29u, 31u, 37u, 41u, 43u,
        // Next segment
        0u, 0u, 0u, 1u, // seqence number
        0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, // file size
        0u, 0u, 0u, 0u, // end of WAL
        // zero padding rest of the segment (16 bytes)
        0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
        0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u
      },
      wal.fd());
  check_file_equals({}, wal);
}

int main() {
  return UnitTest::RunAllTests();
}
