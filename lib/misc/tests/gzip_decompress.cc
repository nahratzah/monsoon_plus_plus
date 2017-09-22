#include <monsoon/io/stream.h>
#include <monsoon/io/gzip_stream.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <vector>

/*
 * echo "chocoladevla" | gzip -9
 */
const std::array<std::uint8_t, 0x21> compressed{{
  0x1f, 0x8b, 0x08, 0x00, 0xa5, 0x5e, 0xc5, 0x59,
  0x02, 0x03, 0x4b, 0xce, 0xc8, 0x4f, 0xce, 0xcf,
  0x49, 0x4c, 0x49, 0x2d, 0xcb, 0x49, 0xe4, 0x02,
  0x00, 0x34, 0x40, 0xad, 0x13, 0x0d, 0x00, 0x00,
  0x00
}};

class mock_reader
: public monsoon::io::stream_reader
{
 public:
  mock_reader(const std::uint8_t* b, const std::uint8_t* e)
  : b_(b),
    e_(e)
  {}

  std::size_t read(void* buf, std::size_t len) override {
    std::size_t maxlen = e_ - b_;
    auto rlen = std::min(maxlen, len);
    std::copy_n(b_, rlen, reinterpret_cast<std::uint8_t*>(buf));
    b_ += rlen;
    return rlen;
  }

  void close() override {}

  bool at_end() const override {
    return b_ == e_;
  }

 private:
  const std::uint8_t* b_;
  const std::uint8_t* e_;
};

int main() {
  std::vector<std::uint8_t> output;

  monsoon::io::gzip_decompress_reader<mock_reader> gzip =
      mock_reader(compressed.begin(), compressed.end());
  while (!gzip.at_end()) {
    std::array<std::uint8_t, 16> buf;
    auto rlen = gzip.read(buf.data(), buf.size());
    std::copy(buf.begin(), buf.begin() + rlen, std::back_inserter(output));
  }
  gzip.close();

  std::cout << std::string(output.begin(), output.end());
}
