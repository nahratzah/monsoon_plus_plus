#ifndef V2_ENCDEC_CTX_H
#define V2_ENCDEC_CTX_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <monsoon/io/fd.h>
#include <monsoon/xdr/xdr_stream.h>
#include <monsoon/io/stream.h>
#include <monsoon/io/ptr_stream.h>
#include "file_segment_ptr.h"

namespace monsoon::history::v2 {
namespace header_flags {


monsoon_dirhistory_local_
constexpr std::uint32_t /* KIND: indicate if the type of file data. */
                        KIND_MASK        = 0x0000000f,
                        KIND_LIST        = 0x00000000,
                        KIND_TABLES      = 0x00000001,
                        /* Indicates segment compression algorithm. */
                        COMPRESSION_MASK = 0x3f000000,
                        LZO_1X1          = 0x10000000,
                        GZIP             = 0x20000000,
                        SNAPPY           = 0x30000000,
                        /* Set if the file has sorted/unique timestamps. */
                        SORTED           = 0x40000000,
                        DISTINCT         = 0x80000000;


} /* namespace monsoon::history::v2::header_flags */


class monsoon_dirhistory_local_ encdec_ctx {
 public:
  enum class compression_type : std::uint32_t {
    NONE = 0,
    LZO_1X1 = header_flags::LZO_1X1,
    GZIP = header_flags::GZIP,
    SNAPPY = header_flags::SNAPPY
  };

  encdec_ctx() noexcept;
  encdec_ctx(const encdec_ctx&);
  encdec_ctx(encdec_ctx&&) noexcept;
  encdec_ctx& operator=(const encdec_ctx&);
  encdec_ctx& operator=(encdec_ctx&&) noexcept;
  encdec_ctx(std::shared_ptr<io::fd>, std::uint32_t) noexcept;
  ~encdec_ctx() noexcept;

  const std::shared_ptr<io::fd>& fd() const noexcept { return fd_; }
  std::uint32_t flags() const noexcept { return hdr_flags_; }
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
  std::shared_ptr<io::fd> fd_ = nullptr;
  std::uint32_t hdr_flags_ = 0;
};


} /* namespace monsoon::history::v2 */

#endif /* V2_ENCDEC_CTX_H */
