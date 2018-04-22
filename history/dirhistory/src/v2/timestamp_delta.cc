#include "timestamp_delta.h"
#include <algorithm>
#include <iterator>

namespace monsoon::history::v2 {


auto timestamp_delta::from_xdr(xdr::xdr_istream& in)
-> timestamp_delta {
  timestamp_delta result;
  result.decode(in);
  return result;
}

auto timestamp_delta::decode(xdr::xdr_istream& in) -> void {
  clear();
  push_back(time_point(in.get_int64()));

  in.get_collection(
      [](xdr::xdr_istream& in) {
        return time_point(in.get_int32());
      },
      *this);

  for (iterator i = std::next(begin()), i_end = end();
      i != i_end;
      ++i) {
    *i += time_point::duration(std::prev(i)->millis_since_posix_epoch());
  }
}

auto timestamp_delta::encode(xdr::xdr_ostream& out) const -> void {
  if (empty())
    throw std::invalid_argument("empty time_point collection");

  std::int64_t pred = front().millis_since_posix_epoch();
  out.put_int64(pred);

  out.put_collection(
      [&pred](xdr::xdr_ostream& out, time_point tp) {
        const std::int64_t tp_millis = tp.millis_since_posix_epoch();
        const std::int64_t delta = tp_millis - pred;
        pred = tp_millis;

        if (delta > 0x7fffffff || delta < -0x80000000) {
          throw std::invalid_argument("time between successive timestamps "
              "is too large");
        }
        out.put_int32(delta);
      },
      std::next(begin()), end());
}


} /* namespace monsoon::history::v2 */
