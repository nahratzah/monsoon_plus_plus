#ifndef V1_TSDATA_INL_H
#define V1_TSDATA_INL_H

#include "../tsdata_mime.h"

namespace monsoon {
namespace history {
namespace v1 {


template<typename Fn>
void tsdata_v1::visit(Fn fn) const {
  const auto r = make_xdr_istream(true);
  auto hdr = tsfile_mimeheader(*r); // Decode and discard.
  decode_tsfile_header(*r); // Decode and discard.

  dictionary_delta dict;
  while (!r->at_end())
    std::invoke(fn, decode_time_series(*r, dict));
  r->close();
}


}}} /* namespace monsoon::history::v1 */

#endif /* V1_TSDATA_INL_H */
