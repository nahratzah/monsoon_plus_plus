#ifndef V2_ENCDEC_CTX_H
#define V2_ENCDEC_CTX_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <monsoon/io/fd.h>
#include <monsoon/xdr/xdr_stream.h>
#include <monsoon/io/stream.h>
#include <monsoon/io/ptr_stream.h>
#include "file_segment_ptr.h"
#include "tsfile_header.h"
#include <cassert>
#include <cstdint>

namespace monsoon::history::v2 {


class monsoon_dirhistory_local_ encdec_ctx {
 public:
  enum class compression_type : std::uint32_t {
    NONE = 0,
    LZO_1X1 = header_flags::LZO_1X1,
    GZIP = header_flags::GZIP,
    SNAPPY = header_flags::SNAPPY
  };

  constexpr encdec_ctx() noexcept = default;

  constexpr encdec_ctx(io::fd& fd, std::uint32_t hdr_flags)
  : fd_(&fd),
    hdr_flags_(hdr_flags)
  {}

  auto fd() const noexcept -> io::fd& {
    assert(fd_ != nullptr);
    return *fd_;
  }

  auto flags() const noexcept -> std::uint32_t { return hdr_flags_; }

  auto new_reader(const file_segment_ptr&, bool = true) const
      -> xdr::xdr_stream_reader<io::ptr_stream_reader>;
  auto decompress(io::ptr_stream_reader&&, bool) const
      -> std::unique_ptr<io::stream_reader>;
  auto compress(io::ptr_stream_writer&&) const
      -> std::unique_ptr<io::stream_writer>;

  compression_type compression() const noexcept {
    return compression_type(hdr_flags_ & header_flags::COMPRESSION_MASK);
  }

 private:
  io::fd* fd_ = nullptr;
  std::uint32_t hdr_flags_ = 0;
};


} /* namespace monsoon::history::v2 */

#endif /* V2_ENCDEC_CTX_H */
