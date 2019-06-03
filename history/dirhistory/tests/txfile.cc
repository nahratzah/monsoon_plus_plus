#include "print.h"
#include "UnitTest++/UnitTest++.h"
#include <monsoon/history/dir/io/txfile.h>
#include <monsoon/io/fd.h>
#include <string>
#include <string_view>

using monsoon::history::io::txfile;
constexpr std::size_t WAL_SIZE = 4u << 20;

namespace {

auto read(const txfile& f) -> std::string {
  auto tx = f.begin();
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

  [[unreachable]];
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

int main() {
  return UnitTest::RunAllTests();
}
