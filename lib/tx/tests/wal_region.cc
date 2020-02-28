#include "print.h"
#include "UnitTest++/UnitTest++.h"
#include <monsoon/tx/detail/wal.h>
#include <monsoon/io/fd.h>
#include <monsoon/io/positional_stream.h>
#include <stdexcept>
#include <string_view>

using monsoon::tx::detail::wal_region;

namespace {

template<typename FD>
void write_all_at(FD& fd, monsoon::io::fd::offset_type off, const void* buf, std::size_t len) {
  while (len > 0) {
    const auto wlen = fd.write_at(off, buf, len);
    buf = reinterpret_cast<const std::uint8_t*>(buf) + wlen;
    len -= wlen;
    off += wlen;
  }
}

template<typename FD>
auto set_file_contents(FD&& fd, std::vector<std::uint8_t> v) -> FD&& {
  fd.truncate(v.size());
  write_all_at(fd, 0, v.data(), v.size());
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

auto file_contents(const wal_region::tx& tx) -> std::vector<std::uint8_t> {
  std::vector<std::uint8_t> v;
  if (tx.size() > v.max_size()) throw std::length_error("WAL file too large for vector");
  v.resize(tx.size());

  auto buf = v.data();
  auto len = v.size();
  monsoon::io::fd::offset_type off = 0;
  while (len > 0) {
    const auto rlen = tx.read_at(off, buf, len);
    buf += rlen;
    len -= rlen;
    off += rlen;
  }

  return v;
}

template<typename FD>
void check_file_equals(std::vector<std::uint8_t> expect, const FD& fd, std::size_t off = 0) {
  CHECK_EQUAL(expect.size() + off, fd.size());
  const auto fd_v = file_contents(fd);
  if (expect.size() + off == fd_v.size())
    CHECK_ARRAY_EQUAL(expect.data() + off, fd_v.data() + off, expect.size());
}

template<typename FD>
void check_file_equals(std::string_view expect, const FD& fd, std::size_t off = 0) {
  CHECK_EQUAL(expect.size() + off, fd.size());
  const auto fd_v = file_contents(fd);
  const auto fd_s = std::string_view(reinterpret_cast<const char*>(fd_v.data()), fd_v.size()).substr(off);
  CHECK_EQUAL(expect, fd_s);
}

template<typename FD>
void check_file_empty(const FD& fd) {
  CHECK_EQUAL(0u, fd.size());
  const auto fd_v = file_contents(fd);
  std::uint8_t expect[1];
  CHECK_ARRAY_EQUAL(expect, fd_v.data(), 0);
}

} /* namespace <unnamed> */

#define TMPFILE() monsoon::io::fd::tmpfile(__FILE__)
#define TMPFILE_WITH_DATA(data) set_file_contents(TMPFILE(), data)

TEST(new_file) {
  auto wal = wal_region("waltest", wal_region::create(), TMPFILE(), 0, 64);

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
  check_file_empty(wal);
}

TEST(existing_file_read_write) {
  auto wal = wal_region(
      "waltest",
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
  check_file_empty(wal);
}

TEST(resize_and_commit) {
  auto wal = std::make_shared<wal_region>("waltest", wal_region::create(), TMPFILE(), 0, 256);
  auto tx = wal_region::tx(wal);

  check_file_empty(tx);
  tx.resize(3);
  check_file_equals({ 0u, 0u, 0u }, tx);
  tx.commit();

  check_file_equals({ 0u, 0u, 0u }, *wal);
}

TEST(write_and_commit) {
  auto wal = std::make_shared<wal_region>("waltest", wal_region::create(), TMPFILE(), 0, 256);
  auto tx = wal_region::tx(wal);

  tx.resize(8);
  tx.write_at(0, u8"01234567", 8);
  check_file_equals(u8"01234567", tx);
  tx.commit();

  check_file_equals(u8"01234567", *wal);
}

TEST(write_but_dont_commit) {
  auto wal = std::make_shared<wal_region>("waltest", wal_region::create(), TMPFILE(), 0, 256);
  auto tx = wal_region::tx(wal);

  tx.resize(8);
  tx.write_at(0, u8"01234567", 8);
  check_file_equals(u8"01234567", tx);

  check_file_equals(u8"", *wal);
}

TEST(write_and_commit_and_compact) {
  auto wal = std::make_shared<wal_region>("waltest", wal_region::create(), TMPFILE(), 0, 256);
  auto tx = wal_region::tx(wal);

  tx.resize(8);
  tx.write_at(0, u8"01234567", 8);
  tx.commit();
  wal->compact();

  check_file_equals(u8"01234567", *wal);
  check_file_equals(u8"01234567", wal->fd(), 256);
}

TEST(write_and_commit_and_reopen) {
  auto wal = std::make_shared<wal_region>("waltest", wal_region::create(), TMPFILE(), 0, 256);
  auto tx = wal_region::tx(wal);

  tx.resize(8);
  tx.write_at(0, u8"01234567", 8);
  tx.commit();

  wal = std::make_shared<wal_region>("waltest", std::move(*wal).fd(), 0, 256);

  check_file_equals(u8"01234567", *wal);
  check_file_equals(u8"01234567", wal->fd(), 256);
}

TEST(commit_order_matters) {
  auto wal = std::make_shared<wal_region>("waltest", wal_region::create(), TMPFILE(), 0, 256);
  {
    // We need three bytes of space.
    auto r = wal_region::tx(wal);
    r.resize(3);
    r.commit();
  }

  auto tx1 = wal_region::tx(wal);
  tx1.write_at(0, u8"one", 3);

  auto tx2 = wal_region::tx(wal);
  tx2.write_at(0, u8"two", 3);

  tx2.commit(); // writes "two" at the position
  tx1.commit(); // writes "one" at the position

  check_file_equals(u8"one", *wal);
}

TEST(commit_order_matters_on_reopen) {
  auto wal = std::make_shared<wal_region>("waltest", wal_region::create(), TMPFILE(), 0, 256);
  {
    // We need three bytes of space.
    auto r = wal_region::tx(wal);
    r.resize(3);
    r.commit();
  }

  auto tx1 = wal_region::tx(wal);
  tx1.write_at(0, u8"one", 3);

  auto tx2 = wal_region::tx(wal);
  tx2.write_at(0, u8"two", 3);

  tx2.commit(); // writes "two" at the position
  tx1.commit(); // writes "one" at the position

  // Reopen.
  wal = std::make_shared<wal_region>("waltest", std::move(*wal).fd(), 0, 256);
  check_file_equals(u8"one", *wal);
}

int main() {
  return UnitTest::RunAllTests();
}
