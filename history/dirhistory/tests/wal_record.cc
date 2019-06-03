#include "print.h"
#include "UnitTest++/UnitTest++.h"
#include <monsoon/history/dir/io/wal.h>
#include <monsoon/xdr/xdr.h>

using monsoon::history::io::wal_record;
using monsoon::history::io::wal_entry;

namespace std {

auto operator<<(std::ostream& o, wal_entry e) -> std::ostream& {
  switch (e) {
    default:
      o << "(" << static_cast<std::uint8_t>(e) + 0u << ")";
      break;
    case wal_entry::end:
      o << "wal_entry::end";
      break;
    case wal_entry::commit:
      o << "wal_entry::commit";
      break;
    case wal_entry::write:
      o << "wal_entry::write";
      break;
    case wal_entry::resize:
      o << "wal_entry::resize";
      break;
  }
  return o;
}

}

auto wal_record_as_bytes(const wal_record& r) -> std::vector<std::uint8_t> {
  monsoon::xdr::xdr_bytevector_ostream<> x;
  r.write(x);
  return x.as_vector();
}

TEST(wal_end_must_be_all_zeroes) {
  monsoon::xdr::xdr_bytevector_ostream<> x;

  wal_record::make_end()->write(x);

  CHECK_EQUAL(std::vector<std::uint8_t>({ 0u, 0u, 0u, 0u }), x.as_vector());
}

TEST(wal_end) {
  auto ptr = wal_record::make_end();

  CHECK_EQUAL(wal_entry::end, ptr->get_wal_entry());
  CHECK_EQUAL(true, ptr->is_end());
  CHECK_EQUAL(false, ptr->is_commit());
  CHECK_EQUAL(true, ptr->is_control_record());
  CHECK_EQUAL(0u, ptr->tx_id());
  CHECK_EQUAL(std::vector<std::uint8_t>({ 0u, 0u, 0u, 0u }), wal_record_as_bytes(*ptr));
}

TEST(wal_commit) {
  auto ptr = wal_record::make_commit(16);

  CHECK_EQUAL(wal_entry::commit, ptr->get_wal_entry());
  CHECK_EQUAL(false, ptr->is_end());
  CHECK_EQUAL(true, ptr->is_commit());
  CHECK_EQUAL(false, ptr->is_control_record());
  CHECK_EQUAL(16u, ptr->tx_id());
  CHECK_EQUAL(std::vector<std::uint8_t>({ 0u, 0u, 16u, 1u }), wal_record_as_bytes(*ptr));
}

TEST(wal_write) {
  auto ptr = wal_record::make_write(17, 0x1234, std::vector<std::uint8_t>{ 47u, 48u, 49u });

  CHECK_EQUAL(wal_entry::write, ptr->get_wal_entry());
  CHECK_EQUAL(false, ptr->is_end());
  CHECK_EQUAL(false, ptr->is_commit());
  CHECK_EQUAL(false, ptr->is_control_record());
  CHECK_EQUAL(17u, ptr->tx_id());
  CHECK_EQUAL(std::vector<std::uint8_t>({
          0u, 0u, 17u, 10u, // 3-byte tx_id, 1-byte record type
          0u, 0u, 0u, 0u, 0u, 0u, 0x12u, 0x34u, // 8-byte offset
          0u, 0u, 0u, 3u, // 4-byte length
          47u, 48u, 49u, 0u, // 3-byte data, 1-byte padding
      }), wal_record_as_bytes(*ptr));
}

TEST(wal_resize) {
  auto ptr = wal_record::make_resize(17, 0x12345678u);

  CHECK_EQUAL(wal_entry::resize, ptr->get_wal_entry());
  CHECK_EQUAL(false, ptr->is_end());
  CHECK_EQUAL(false, ptr->is_commit());
  CHECK_EQUAL(17u, ptr->tx_id());
  CHECK_EQUAL(std::vector<std::uint8_t>({
          0u, 0u, 17u, 11u, // 3-byte tx_id, 1-byte record type
          0u, 0u, 0u, 0u, 0x12u, 0x34u, 0x56u, 0x78u // 8-byte new-size
      }), wal_record_as_bytes(*ptr));
}

int main(int argc, char** argv) {
  return UnitTest::RunAllTests();
}
