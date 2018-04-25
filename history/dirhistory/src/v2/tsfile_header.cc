#include "tsfile_header.h"
#include "xdr_primitives.h"

namespace monsoon::history::v2 {


auto tsfile_header::decode(xdr::xdr_istream& in)
-> void {
  first = decode_timestamp(in);
  last = decode_timestamp(in);
  flags = in.get_uint32();
  reserved = in.get_uint32();
  file_size = in.get_uint64();
  fdt.decode(in);
}

auto tsfile_header::encode(xdr::xdr_ostream& out) const
-> void {
  encode_timestamp(out, first);
  encode_timestamp(out, last);
  out.put_uint32(flags);
  out.put_uint32(reserved);
  out.put_uint64(file_size);
  fdt.encode(out);
}


} /* namespace monsoon::history::v2 */
