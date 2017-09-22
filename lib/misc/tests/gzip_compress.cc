#include <monsoon/io/fd.h>
#include <monsoon/io/positional_stream.h>
#include <monsoon/io/gzip_stream.h>
#include <array>
#include <cstdint>
#include <iostream>
#include <string>

void compress(monsoon::io::fd& file) {
  std::string contents = "No cats were harmed in the making of this test.";
  auto remaining = contents.length();
  auto buf = contents.data();

  auto w = monsoon::io::gzip_compress_writer<monsoon::io::positional_writer>(monsoon::io::positional_writer(file));
  while (remaining != 0) {
    auto wlen = w.write(buf, remaining);
    buf += wlen;
    remaining -= wlen;
  }
  w.close();
}

std::string decompress(monsoon::io::fd& file) {
  std::string contents;

  auto r = monsoon::io::gzip_decompress_reader<monsoon::io::positional_reader>(monsoon::io::positional_reader(file), true);
  while (!r.at_end()) {
    std::array<std::uint8_t, 16> buf;
    auto rlen = r.read(buf.data(), buf.size());
    contents.append(buf.begin(), buf.begin() + rlen);
  }
  r.close();

  return contents;
}

int main() {
  auto file = monsoon::io::fd::tmpfile("gzip_compress_test");

  compress(file);
  std::cout << decompress(file);
}
