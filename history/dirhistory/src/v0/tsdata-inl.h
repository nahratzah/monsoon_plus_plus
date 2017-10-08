#ifndef V0_TSDATA_INL_H
#define V0_TSDATA_INL_H

#include "../tsdata_mime.h"

namespace monsoon {
namespace history {
namespace v0 {


template<typename Fn>
void tsdata_v0::visit(Fn fn) const {
  const auto r = make_xdr_istream(true);
  auto hdr = tsfile_mimeheader(*r); // Decode and discard.
  decode_tsfile_header(*r); // Decode and discard.

  while (!r->at_end())
    std::invoke(fn, decode_time_series(*r));
  r->close();
}


}}} /* namespace monsoon::history::v0 */

#endif /* V0_TSDATA_INL_H */
