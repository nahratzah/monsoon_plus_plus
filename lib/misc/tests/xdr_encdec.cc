#include <monsoon/xdr/xdr.h>
#include <monsoon/xdr/xdr_stream.h>
#include <monsoon/io/stream.h>
#include <algorithm>
#include <initializer_list>
#include <utility>
#include <vector>
#include "UnitTest++/UnitTest++.h"

class mock_reader
: public monsoon::io::stream_reader
{
 public:
  mock_reader(std::vector<std::uint8_t>&& l) : data_(std::move(l)) {}

  std::size_t read(void* buf, std::size_t len) override {
    auto rlen = std::min(data_.size(), len);
    std::copy_n(data_.begin(), rlen, reinterpret_cast<std::uint8_t*>(buf));
    data_.erase(data_.begin(), data_.begin() + rlen);
    return rlen;
  }

  bool at_end() const override { return data_.empty(); }

  void close() override {}

 private:
  std::vector<std::uint8_t> data_;
};

auto make_xdr_reader(std::initializer_list<unsigned int> il)
-> monsoon::xdr::xdr_stream_reader<mock_reader>
{
  std::vector<std::uint8_t> data;
  std::copy(il.begin(), il.end(), std::back_inserter(data));
  return monsoon::xdr::xdr_stream_reader<mock_reader>(
      mock_reader(std::move(data)));
}

TEST(xdr_void_decode) {
  make_xdr_reader({}).get_void();
}

TEST(xdr_bool_decode) {
  CHECK_EQUAL(true, make_xdr_reader({ 0u, 0u, 0u, 1u }).get_bool());
  CHECK_EQUAL(false, make_xdr_reader({ 0u, 0u, 0u, 0u }).get_bool());
}

TEST(xdr_uint8_decode) {
  CHECK_EQUAL(7u, make_xdr_reader({ 0u, 0u, 0u, 7u }).get_uint8());
  CHECK_EQUAL(255u, make_xdr_reader({ 0u, 0u, 0u, 255u }).get_uint8());
}

TEST(xdr_int8_decode) {
  CHECK_EQUAL(7, make_xdr_reader({ 0u, 0u, 0u, 7u }).get_int8());
  CHECK_EQUAL(0x7f, make_xdr_reader({ 0u, 0u, 0u, 0x7fu }).get_int8());
  CHECK_EQUAL(-128, make_xdr_reader({ 0xffu, 0xffu, 0xffu, 0x80u }).get_int8());
  CHECK_EQUAL(-1, make_xdr_reader({ 0xffu, 0xffu, 0xffu, 255u }).get_int8());
}

TEST(xdr_uint16_decode) {
  CHECK_EQUAL(65535u, make_xdr_reader({ 0u, 0u, 0xffu, 0xffu }).get_uint16());
  CHECK_EQUAL(256u, make_xdr_reader({ 0u, 0u, 1u, 0u }).get_uint16());
  CHECK_EQUAL(19u, make_xdr_reader({ 0u, 0u, 0u, 19u }).get_uint16());
}

TEST(xdr_int16_decode) {
  CHECK_EQUAL(-32768, make_xdr_reader({ 0xffu, 0xffu, 0x80u, 0x00u }).get_int16());
  CHECK_EQUAL(-1, make_xdr_reader({ 0xffu, 0xffu, 0xffu, 0xffu }).get_int16());
  CHECK_EQUAL(256, make_xdr_reader({ 0u, 0u, 1u, 0u }).get_int16());
  CHECK_EQUAL(19, make_xdr_reader({ 0u, 0u, 0u, 19u }).get_int16());
}

TEST(xdr_uint32_decode) {
  CHECK_EQUAL(0xff00ff00u, make_xdr_reader({ 0xff, 0, 0xff, 0 }).get_uint32());
  CHECK_EQUAL(0x11223344u, make_xdr_reader({ 0x11, 0x22, 0x33, 0x44 }).get_uint32());
  CHECK_EQUAL(0u, make_xdr_reader({ 0, 0, 0, 0 }).get_uint32());
}

TEST(xdr_int32_decode) {
  CHECK_EQUAL((int)0xff00ff00, make_xdr_reader({ 0xff, 0, 0xff, 0 }).get_int32());
  CHECK_EQUAL(0x11223344, make_xdr_reader({ 0x11, 0x22, 0x33, 0x44 }).get_int32());
  CHECK_EQUAL(0, make_xdr_reader({ 0, 0, 0, 0 }).get_int32());
  CHECK_EQUAL(0, make_xdr_reader({ 0, 0, 0, 0 }).get_int32());
}

TEST(xdr_uint64_decode) {
  CHECK_EQUAL(0xffeeddccbbaa9988ul, make_xdr_reader({ 0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88 }).get_uint64());
  CHECK_EQUAL(0ul, make_xdr_reader({ 0, 0, 0, 0, 0, 0, 0, 0 }).get_uint64());
}

TEST(xdr_int64_decode) {
  CHECK_EQUAL(0x1122334455667788l, make_xdr_reader({ 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 }).get_int64());
  CHECK_EQUAL(-1l, make_xdr_reader({ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }).get_int64());
  CHECK_EQUAL(0xffffffffl, make_xdr_reader({ 0, 0, 0, 0, 0xff, 0xff, 0xff, 0xff }).get_int64());
}

TEST(xdr_string_decode) {
  CHECK_EQUAL("bla", make_xdr_reader({ 0, 0, 0, 3, 'b', 'l', 'a', 0 }).get_string());
  CHECK_EQUAL("", make_xdr_reader({ 0, 0, 0, 0 }).get_string());
  CHECK_EQUAL("bla\x01", make_xdr_reader({ 0, 0, 0, 4, 'b', 'l', 'a', 1 }).get_string());
}

int main() {
  return UnitTest::RunAllTests();
}
