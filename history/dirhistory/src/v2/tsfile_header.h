#ifndef V2_TSFILE_HEADER_H
#define V2_TSFILE_HEADER_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <cstddef>
#include <cstdint>
#include <monsoon/time_point.h>
#include <monsoon/xdr/xdr.h>
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


class monsoon_dirhistory_local_ tsfile_header {
 public:
  static constexpr std::size_t XDR_SIZE = 16 + 4 + 4 + 8 + 16;

  std::uint32_t kind() const noexcept { return flags & header_flags::KIND_MASK; }

  auto decode(xdr::xdr_istream& in) -> void;
  auto encode(xdr::xdr_ostream& out) const -> void;

  time_point first, last; // 16 bytes (8 each)
  std::uint32_t flags = 0; // 4 bytes
  std::uint32_t reserved = 0; // 4 bytes
  std::uint64_t file_size = 0; // 8 bytes
  file_segment_ptr fdt; // 16 bytes (underlying file pointer)
};


} /* namespace monsoon::history::v2 */

#endif /* V2_TSFILE_HEADER_H */
