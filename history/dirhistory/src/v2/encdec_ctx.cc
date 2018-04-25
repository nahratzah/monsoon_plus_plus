#include "encdec_ctx.h"
#include "../raw_file_segment_reader.h"
#include <monsoon/io/gzip_stream.h>

namespace monsoon::history::v2 {


auto encdec_ctx::new_reader(const file_segment_ptr& ptr, bool compression)
  const
-> xdr::xdr_stream_reader<io::ptr_stream_reader> {
  auto rd = io::make_ptr_reader<raw_file_segment_reader>(
      *fd_, ptr.offset(),
      ptr.size());
  if (compression) rd = decompress(std::move(rd), true);
  return xdr::xdr_stream_reader<io::ptr_stream_reader>(std::move(rd));
}

auto encdec_ctx::decompress(io::ptr_stream_reader&& rd, bool validate) const
-> std::unique_ptr<io::stream_reader> {
  switch (compression()) {
    default:
    case compression_type::LZO_1X1: // XXX implement reader
    case compression_type::SNAPPY: // XXX implement reader
      throw std::logic_error("Unsupported compression");
    case compression_type::NONE:
      return std::move(rd).get();
    case compression_type::GZIP:
      return io::new_gzip_decompression(std::move(rd), validate);
  }
}

auto encdec_ctx::compress(io::ptr_stream_writer&& wr) const
-> std::unique_ptr<io::stream_writer> {
  switch (compression()) {
    default:
    case compression_type::LZO_1X1: // XXX implement reader
    case compression_type::SNAPPY: // XXX implement reader
      throw std::logic_error("Unsupported compression");
    case compression_type::NONE:
      return std::move(wr).get();
    case compression_type::GZIP:
      return io::new_gzip_compression(std::move(wr));
  }
}


} /* namespace monsoon::history::v2 */
