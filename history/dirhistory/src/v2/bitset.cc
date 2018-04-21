#include "bitset.h"

namespace monsoon::history::v2 {


void bitset::decode(xdr::xdr_istream& in) {
  clear();

  bool current = true;
  in.accept_collection(
      [](xdr::xdr_istream& in) {
        return in.get_uint16();
      },
      [this, &current](std::uint16_t count) {
        std::fill_n(std::back_inserter(*this), count, current);
        current = !current;
      });
}

void bitset::encode(xdr::xdr_ostream& out) const {
  std::vector<std::uint16_t> counters;

  bool current = true;

  auto bit_iter = begin();
  while (bit_iter != end()) {
    const auto end = std::find(bit_iter, this->end(), !current);
    auto count = end - bit_iter;

    while (count > 0x7fff) {
      counters.push_back(0x7fff);
      counters.push_back(0);
      count -= 0x7fff;
    }
    counters.push_back(count);

    current = !current;
    bit_iter = end;
  }

  out.put_collection(
      [](xdr::xdr_ostream& out, std::uint16_t v) {
        out.put_uint16(v);
      },
      counters.cbegin(), counters.cend());
}


} /* namespace monsoon::history::v2 */
