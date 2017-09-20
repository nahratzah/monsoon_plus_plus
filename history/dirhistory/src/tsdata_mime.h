#ifndef TSDATA_MIME_H
#define TSDATA_MIME_H

#include <cstdint>
#include <monsoon/optional.h>
#include <monsoon/xdr/xdr.h>

namespace monsoon {
namespace history {


class tsfile_mimeheader {
 public:
  static constexpr std::size_t XDR_ENCODED_LEN = 16;
  static constexpr std::array<std::uint8_t, 12> MAGIC = {{
    17u, 19u, 23u, 29u,
    'M', 'O', 'N', '-',
    's', 'o', 'o', 'n'
  }};

  tsfile_mimeheader() noexcept = default;
  tsfile_mimeheader(const tsfile_mimeheader&) noexcept = default;
  tsfile_mimeheader& operator=(const tsfile_mimeheader&) noexcept = default;
  tsfile_mimeheader(std::uint16_t, std::uint16_t) noexcept;

  tsfile_mimeheader(monsoon::xdr::xdr_istream&);

  static auto read(monsoon::xdr::xdr_istream&) -> optional<tsfile_mimeheader>;
  void write(monsoon::xdr::xdr_ostream&) const;

  std::uint16_t major_version, minor_version;
};

class tsfile_badmagic
: public monsoon::xdr::xdr_exception
{
 public:
  ~tsfile_badmagic() override;
};


}} /* namespace monsoon::history */

#include "tsdata_mime-inl.h"

#endif /* TSDATA_MIME_H */
