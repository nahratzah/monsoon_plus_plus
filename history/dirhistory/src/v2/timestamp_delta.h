#ifndef V2_TIMESTAMP_DELTA_H
#define V2_TIMESTAMP_DELTA_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <vector>
#include <monsoon/xdr/xdr.h>
#include <monsoon/time_point.h>
#include <cstdint>

namespace monsoon::history::v2 {


class monsoon_dirhistory_local_ timestamp_delta
: public std::vector<time_point>
{
 public:
  using std::vector<time_point>::vector;
  using std::vector<time_point>::operator=;

  static auto from_xdr(xdr::xdr_istream& in) -> timestamp_delta;
  auto decode(xdr::xdr_istream& in) -> void;
  auto encode(xdr::xdr_ostream& out) const -> void;
};


} /* namespace monsoon::history::v2 */

#endif /* V2_TIMESTAMP_DELTA_H */
