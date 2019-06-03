#include "print.h"
#include "UnitTest++/UnitTest++.h"
#include <monsoon/history/dir/io/txfile.h>
#include <monsoon/io/fd.h>
#include <string>
#include <string_view>

using monsoon::history::io::txfile;
constexpr std::size_t WAL_SIZE = 4u << 20;

namespace {

auto read(const txfile::transaction& tx) -> std::string {
  constexpr std::size_t GROWTH = 8192;

  std::string s;
  monsoon::io::fd::offset_type off = 0;
  for (;;) {
    const auto old_size = s.size();
    s.resize(old_size + GROWTH);

    const auto rlen = tx.read_at(off, &s[old_size], GROWTH);
    s.resize(old_size + rlen);
    off += rlen;
    if (rlen == 0u) return s;
  }
}

auto read(const txfile& f) -> std::string {
  return read(f.begin());
}

void write_all_at(txfile::transaction& tx, monsoon::io::fd::offset_type off, std::string_view s) {
  auto buf = s.data();
  auto len = s.size();
  while (len > 0) {
    const auto wlen = tx.write_at(off, buf, len);
    buf += wlen;
    len -= wlen;
    off += wlen;
  }
}

} /* namespace <unnamed> */

#define TMPFILE() monsoon::io::fd::tmpfile(__FILE__)

TEST(new_file) {
  auto f = txfile::create(TMPFILE(), 0, WAL_SIZE);

  CHECK_EQUAL(u8"", read(f));
}

TEST(write_no_commit) {
  auto f = txfile::create(TMPFILE(), 0, WAL_SIZE);

  auto tx = f.begin(false);
  tx.resize(6);
  write_all_at(tx, 0, u8"foobar");

  CHECK_EQUAL(u8"", read(f));
}

TEST(write_commit) {
  auto f = txfile::create(TMPFILE(), 0, WAL_SIZE);

  auto tx = f.begin(false);
  tx.resize(6);
  write_all_at(tx, 0, u8"foobar");
  tx.commit();

  CHECK_EQUAL(u8"foobar", read(f));
}

TEST(multi_transaction) {
  auto f = txfile::create(TMPFILE(), 0, WAL_SIZE);
  {
    auto tx = f.begin(false);
    tx.resize(1);
    write_all_at(tx, 0, u8"X");
    tx.commit();
  }

  auto tx1 = f.begin(false);
  auto tx2 = f.begin(false);
  auto tx3 = f.begin(false);
  auto ro = f.begin();

  write_all_at(tx1, 0, u8"1");
  write_all_at(tx2, 0, u8"2");
  write_all_at(tx3, 0, u8"3");

  CHECK_EQUAL(u8"1", read(tx1));
  CHECK_EQUAL(u8"2", read(tx2));
  CHECK_EQUAL(u8"3", read(tx3));
  CHECK_EQUAL(u8"X", read(ro));

  tx1.commit();
  tx2.commit();
  tx3.commit();
  ro.rollback();

  CHECK_EQUAL(u8"3", read(f));
}

int main() {
  return UnitTest::RunAllTests();
}
