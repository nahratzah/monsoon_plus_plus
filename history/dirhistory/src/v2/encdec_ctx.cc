#include "encdec_ctx.h"
#include "../raw_file_segment_reader.h"
#include <monsoon/io/gzip_stream.h>

namespace monsoon::history::v2 {


encdec_ctx::encdec_ctx() noexcept {}

encdec_ctx::encdec_ctx(const encdec_ctx& o)
: fd_(o.fd_),
  hdr_flags_(o.hdr_flags_)
{}

encdec_ctx::encdec_ctx(encdec_ctx&& o) noexcept
: fd_(std::move(o.fd_)),
  hdr_flags_(std::move(o.hdr_flags_))
{}

auto encdec_ctx::operator=(const encdec_ctx& o) -> encdec_ctx& {
  fd_ = o.fd_;
  hdr_flags_ = o.hdr_flags_;
  return *this;
}

auto encdec_ctx::operator=(encdec_ctx&& o) noexcept -> encdec_ctx& {
  fd_ = std::move(o.fd_);
  hdr_flags_ = std::move(o.hdr_flags_);
  return *this;
}

encdec_ctx::encdec_ctx(std::shared_ptr<io::fd> fd, std::uint32_t hdr_flags)
  noexcept
: fd_(std::move(fd)),
  hdr_flags_(hdr_flags)
{}

encdec_ctx::~encdec_ctx() noexcept {}

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
