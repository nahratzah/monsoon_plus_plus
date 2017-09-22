#include <monsoon/xdr/xdr.h>
#include <monsoon/xdr/xdr_stream.h>
#include <monsoon/io/stream.h>
#include <algorithm>
#include <initializer_list>
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

auto make_byte_vector(std::initializer_list<unsigned int> il)
-> std::vector<std::uint8_t> {
  std::vector<std::uint8_t> data;
  std::copy(il.begin(), il.end(), std::back_inserter(data));
  return data;
}

auto make_xdr_reader(std::initializer_list<unsigned int> il)
-> monsoon::xdr::xdr_stream_reader<mock_reader>
{
  return monsoon::xdr::xdr_stream_reader<mock_reader>(
      mock_reader(make_byte_vector(il)));
}

auto make_xdr_writer()
-> monsoon::xdr::xdr_bytevector_ostream<> {
  return monsoon::xdr::xdr_bytevector_ostream<>();
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
  CHECK_EQUAL(-1, make_xdr_reader({ 0xff, 0xff, 0xff, 0xff }).get_int32());
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

TEST(xdr_opaque_decode) {
  CHECK_ARRAY_EQUAL(make_byte_vector({ 'A', 'B', 'C' }),
      make_xdr_reader({ 0, 0, 0, 3, 'A', 'B', 'C', 0 }).get_opaque(),
      3);
}

TEST(xdr_collection_decode) {
  CHECK_ARRAY_EQUAL(std::vector<std::string>({ "foo", "foobar" }),
      make_xdr_reader({
          0, 0, 0, 2, // 2 items
          0, 0, 0, 3, // string length
          'f', 'o', 'o', 0, // string contents
          0, 0, 0, 6, // string length
          'f', 'o', 'o', 'b', 'a', 'r', 0, 0 // string contents
      })
      .template get_collection<std::vector<std::string>>(
          [](auto& xdr_in) {
            return xdr_in.get_string();
          }),
      2);
}

TEST(xdr_void_encode) {
  auto xdr = make_xdr_writer();
  xdr.put_void();

  CHECK_EQUAL(0u, xdr.size());
  CHECK_ARRAY_EQUAL(make_byte_vector({}), xdr.as_vector(), 0);
}

TEST(xdr_bool_encode) {
  auto xdr = make_xdr_writer();
  xdr.put_bool(false);
  xdr.put_bool(true);

  CHECK_EQUAL(8u, xdr.size());
  CHECK_ARRAY_EQUAL(
      make_byte_vector({ 0, 0, 0, 0, 0, 0, 0, 1 }),
      xdr.as_vector(),
      8);
}

TEST(xdr_uint8_encode) {
  auto xdr = make_xdr_writer();
  xdr.put_uint8('A');
  xdr.put_uint8(126);
  xdr.put_uint8(255);

  CHECK_EQUAL(12u, xdr.size());
  CHECK_ARRAY_EQUAL(
      make_byte_vector({
          0, 0, 0, 'A',
          0, 0, 0, 126,
          0, 0, 0, 255
      }),
      xdr.as_vector(),
      12);
}

TEST(xdr_int8_encode) {
  auto xdr = make_xdr_writer();
  xdr.put_int8(-128);
  xdr.put_int8(126);

  CHECK_EQUAL(8u, xdr.size());
  CHECK_ARRAY_EQUAL(
      make_byte_vector({
          0xff, 0xff, 0xff, 0x80,
          0, 0, 0, 0x7e
      }),
      xdr.as_vector(),
      8);
}

TEST(xdr_uint16_encode) {
  auto xdr = make_xdr_writer();
  xdr.put_uint16(0xffee);
  xdr.put_uint16(0x4567);

  CHECK_EQUAL(8u, xdr.size());
  CHECK_ARRAY_EQUAL(
      make_byte_vector({
          0x00, 0x00, 0xff, 0xee,
          0, 0, 0x45, 0x67
      }),
      xdr.as_vector(),
      8);
}

TEST(xdr_int16_encode) {
  auto xdr = make_xdr_writer();
  xdr.put_int16(-0x1000);
  xdr.put_int16(0x4567);

  CHECK_EQUAL(8u, xdr.size());
  CHECK_ARRAY_EQUAL(
      make_byte_vector({
          0xff, 0xff, 0xf0, 0x00,
          0, 0, 0x45, 0x67
      }),
      xdr.as_vector(),
      8);
}

TEST(xdr_uint32_encode) {
  auto xdr = make_xdr_writer();
  xdr.put_uint32(0xff00ff00u);
  xdr.put_uint32(0x11223344u);
  xdr.put_uint32(0u);

  CHECK_EQUAL(12u, xdr.size());
  CHECK_ARRAY_EQUAL(
      make_byte_vector({
          0xff, 0, 0xff, 0,
          0x11, 0x22, 0x33, 0x44,
          0, 0, 0, 0
      }),
      xdr.as_vector(),
      12);
}

TEST(xdr_int32_encode) {
  auto xdr = make_xdr_writer();
  xdr.put_int32((int)0xff00ff00);
  xdr.put_int32(0x11223344);
  xdr.put_int32(0);
  xdr.put_int32(-1);

  CHECK_EQUAL(16u, xdr.size());
  CHECK_ARRAY_EQUAL(
      make_byte_vector({
          0xff, 0, 0xff, 0,
          0x11, 0x22, 0x33, 0x44,
          0, 0, 0, 0,
          0xff, 0xff, 0xff, 0xff
      }),
      xdr.as_vector(),
      16);
}

TEST(xdr_uint64_encode) {
  auto xdr = make_xdr_writer();
  xdr.put_uint64(0xffeeddccbbaa9988ul);
  xdr.put_uint64(0ul);

  CHECK_EQUAL(16u, xdr.size());
  CHECK_ARRAY_EQUAL(
      make_byte_vector({
          0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88,
          0, 0, 0, 0, 0, 0, 0, 0
      }),
      xdr.as_vector(),
      16);
}

TEST(xdr_int64_encode) {
  auto xdr = make_xdr_writer();
  xdr.put_int64(0x1122334455667788l);
  xdr.put_int64(-1l);
  xdr.put_int64(0xffffffffl);

  CHECK_EQUAL(24u, xdr.size());
  CHECK_ARRAY_EQUAL(
      make_byte_vector({
          0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
          0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
          0, 0, 0, 0, 0xff, 0xff, 0xff, 0xff
      }),
      xdr.as_vector(),
      24);
}

TEST(xdr_c_string_encode) {
  auto xdr = make_xdr_writer();
  xdr.put_string("c_str()");

  CHECK_EQUAL(12u, xdr.size());
  CHECK_ARRAY_EQUAL(
      make_byte_vector({
          0, 0, 0, 7,
          'c', '_', 's', 't',
          'r', '(', ')', 0
      }),
      xdr.as_vector(),
      12);
}

TEST(xdr_string_encode) {
  using namespace std::literals;

  auto xdr = make_xdr_writer();
  xdr.put_string("foobar"s);

  CHECK_EQUAL(12u, xdr.size());
  CHECK_ARRAY_EQUAL(
      make_byte_vector({
          0, 0, 0, 6,
          'f', 'o', 'o', 'b',
          'a', 'r', 0, 0
      }),
      xdr.as_vector(),
      12);
}

TEST(xdr_opaque_encode) {
  auto xdr = make_xdr_writer();
  xdr.put_opaque(make_byte_vector({ 'f', 'o', 'o', 'b', 'a', 'r' }));

  CHECK_EQUAL(12u, xdr.size());
  CHECK_ARRAY_EQUAL(
      make_byte_vector({
          0, 0, 0, 6,
          'f', 'o', 'o', 'b',
          'a', 'r', 0, 0
      }),
      xdr.as_vector(),
      12);
}

TEST(put_collection) {
  std::vector<std::string> out_collection;
  out_collection.push_back("last");
  out_collection.push_back("test");
  out_collection.push_back("\\o/");

  auto xdr = make_xdr_writer();
  xdr.put_collection(
      [](auto& xdr_out, const auto& elem) {
        xdr_out.put_string(elem);
      },
      out_collection.cbegin(),
      out_collection.cend());

  CHECK_EQUAL(28u, xdr.size());
  CHECK_ARRAY_EQUAL(
      make_byte_vector({
          0, 0, 0, 3, // 3 elements in collection
          0, 0, 0, 4, // 4 bytes in first string
          'l', 'a', 's', 't', // contents of first string
          0, 0, 0, 4, // 4 bytes in second string
          't', 'e', 's', 't', // contents of second string
          0, 0, 0, 3, // 3 bytes in third string
          '\\', 'o', '/', 0 // contents of third string
      }),
      xdr.as_vector(),
      28);
}

int main() {
  return UnitTest::RunAllTests();
}
