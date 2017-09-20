#ifndef TSDATA_MIME_H
#define TSDATA_MIME_H

#include <cstdint>
#include <monsoon/xdr/xdr.h>

namespace monsoon {
namespace history {


class tsfile_mimeheader {
 public:
  static const std::array<std::uint8_t, 12> MAGIC;

  tsfile_mimeheader() noexcept = default;
  tsfile_mimeheader(const tsfile_mimeheader&) noexcept = default;
  tsfile_mimeheader& operator=(const tsfile_mimeheader&) noexcept = default;
  tsfile_mimeheader(std::uint16_t, std::uint16_t) noexcept;

  tsfile_mimeheader(monsoon::xdr::xdr_istream&);

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

#endif /* TSDATA_MIME_H */
