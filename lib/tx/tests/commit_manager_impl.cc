#include "print.h"
#include "UnitTest++/UnitTest++.h"
#include <monsoon/tx/detail/commit_manager.h>
#include <monsoon/tx/detail/commit_manager_impl.h>
#include <monsoon/tx/txfile.h>
#include <monsoon/io/rw.h>
#include <cstddef>
#include <system_error>
#include <vector>
#include <boost/endian/conversion.hpp>


using monsoon::tx::detail::commit_manager;
using monsoon::tx::detail::commit_manager_impl;

namespace {

auto read(const monsoon::tx::txfile::transaction& tx) -> std::vector<std::uint8_t> {
  constexpr std::size_t GROWTH = 8192;

  std::vector<std::uint8_t> s;
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

auto read(const monsoon::tx::txfile& f) -> std::vector<std::uint8_t> {
  return read(f.begin());
}

void check_file_equals(const std::vector<std::uint8_t>& expect, const monsoon::tx::txfile& fd) {
  const std::vector<std::uint8_t> fd_v = read(fd);
  CHECK_EQUAL(expect.size(), fd_v.size());
  if (expect.size() == fd_v.size())
    CHECK_ARRAY_EQUAL(expect.data(), fd_v.data(), expect.size());
}

auto tmp_txfile(std::string name) -> monsoon::tx::txfile {
  return monsoon::tx::txfile::create(name, monsoon::io::fd::tmpfile(__FILE__), 0, 4u << 20);
}
#define tmp_txfile() tmp_txfile(__func__)

auto file_with_inits(
    monsoon::tx::txfile&& f,
    commit_manager::type tx_start,
    commit_manager::type last_write,
    commit_manager::type completed_commit) -> monsoon::tx::txfile&& {
  std::uint32_t magic = commit_manager_impl::magic;

  boost::endian::native_to_big_inplace(magic);
  boost::endian::native_to_big_inplace(tx_start);
  boost::endian::native_to_big_inplace(last_write);
  boost::endian::native_to_big_inplace(completed_commit);

  auto t = f.begin(false);
  t.resize(16);
  monsoon::io::write_at(t,  0, &magic, sizeof(magic));
  monsoon::io::write_at(t,  4, &tx_start, sizeof(tx_start));
  monsoon::io::write_at(t,  8, &last_write, sizeof(last_write));
  monsoon::io::write_at(t, 12, &completed_commit, sizeof(completed_commit));
  t.commit();

  return std::move(f);
}

} /* namespace <unnamed> */


TEST(new_file) {
  auto f = tmp_txfile();
  {
    auto t = f.begin(false);
    t.resize(commit_manager_impl::SIZE);
    commit_manager_impl::init(t, 0);
    t.commit();
  }

  check_file_equals(
      {
        0x69, 0x7f, 0x64, 0x31,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
      },
      f);
}

TEST(commit_manager_knows_of_commit_manager_impl) {
  auto f = file_with_inits(tmp_txfile(), 1, 17, 15);
  auto cm = commit_manager::allocate(f, 0);

  REQUIRE CHECK(cm != nullptr);
}

TEST(get_tx_commit_id) {
  auto f = file_with_inits(tmp_txfile(), 1, 17, 15);
  auto cm = commit_manager_impl::allocate(f, 0);
  REQUIRE CHECK(cm != nullptr);
  auto ci = cm->get_tx_commit_id();

  CHECK_EQUAL( 1, ci.tx_start());
  CHECK_EQUAL(15, ci.val());
}

TEST(get_tx_commit_id_repeats) {
  auto f = file_with_inits(tmp_txfile(), 1, 17, 15);
  auto cm = commit_manager_impl::allocate(f, 0);
  REQUIRE CHECK(cm != nullptr);

  auto ci1 = cm->get_tx_commit_id();
  auto ci2 = cm->get_tx_commit_id();

  CHECK_EQUAL(ci1, ci2);
}

TEST(prepare_commit) {
  auto f = file_with_inits(tmp_txfile(), 1, 17, 15);
  auto cm = commit_manager_impl::allocate(f, 0);
  REQUIRE CHECK(cm != nullptr);
  const auto ci_before = cm->get_tx_commit_id();

  auto wi = cm->prepare_commit(f);
  CHECK_EQUAL( 1, wi.seq().tx_start());
  CHECK_EQUAL(18, wi.seq().val());

  CHECK_EQUAL(ci_before, cm->get_tx_commit_id()); // Doesn't change read ID.

  CHECK(wi.seq() != cm->prepare_commit(f).seq()); // Doesn't hand out the same ID twice.
}

TEST(commit) {
  auto f = file_with_inits(tmp_txfile(), 1, 17, 15);
  auto cm = commit_manager_impl::allocate(f, 0);
  REQUIRE CHECK(cm != nullptr);

  auto wi = cm->prepare_commit(f);
  REQUIRE CHECK_EQUAL( 1, wi.seq().tx_start());
  REQUIRE CHECK_EQUAL(18, wi.seq().val());

  int validation_called = 0;
  int phase2_called = 0;
  auto ec = wi.apply(
      [&validation_called, &phase2_called]() -> std::error_code {
        CHECK_EQUAL(0, validation_called);
        CHECK_EQUAL(0, phase2_called);
        ++validation_called;
        return {};
      },
      [&validation_called, &phase2_called]() noexcept {
        CHECK_EQUAL(1, validation_called);
        CHECK_EQUAL(0, phase2_called);
        ++phase2_called;
      });
  // Apply function returned the error code.
  CHECK_EQUAL(std::error_code(), ec);
  CHECK_EQUAL(1, validation_called);
  CHECK_EQUAL(1, phase2_called);

  // New read transactions now use the commited transaction.
  auto cid_after_commit = cm->get_tx_commit_id();
  CHECK_EQUAL( 1, wi.seq().tx_start());
  CHECK_EQUAL(18, wi.seq().val());
}

TEST(failed_commit) {
  auto f = file_with_inits(tmp_txfile(), 1, 17, 15);
  auto cm = commit_manager_impl::allocate(f, 0);
  REQUIRE CHECK(cm != nullptr);
  const auto ci_before = cm->get_tx_commit_id();

  auto wi = cm->prepare_commit(f);
  REQUIRE CHECK_EQUAL( 1, wi.seq().tx_start());
  REQUIRE CHECK_EQUAL(18, wi.seq().val());

  int validation_called = 0;
  int phase2_called = 0;
  auto ec = wi.apply(
      [&validation_called, &phase2_called]() -> std::error_code {
        CHECK_EQUAL(0, validation_called);
        CHECK_EQUAL(0, phase2_called);
        ++validation_called;
        return std::make_error_code(std::errc::no_link);
      },
      [&validation_called, &phase2_called]() noexcept {
        CHECK_EQUAL(1, validation_called);
        CHECK_EQUAL(0, phase2_called);
        ++phase2_called;
      });
  // Apply function returned the error code.
  CHECK_EQUAL(std::make_error_code(std::errc::no_link), ec);
  CHECK_EQUAL(1, validation_called);
  CHECK_EQUAL(0, phase2_called);

  // Canceled transaction doesn't affect read transactions.
  CHECK_EQUAL(ci_before, cm->get_tx_commit_id());
}

int main() {
  return UnitTest::RunAllTests();
}
