#ifndef TSDATA_MIME_INL_H
#define TSDATA_MIME_INL_H

namespace monsoon {
namespace history {


inline tsfile_mimeheader::tsfile_mimeheader(std::uint16_t majver,
    std::uint16_t minver) noexcept
: major_version(majver),
  minor_version(minver)
{}

inline tsfile_mimeheader::tsfile_mimeheader(monsoon::xdr::xdr_istream& in) {
  if (in.template get_bytes<MAGIC.size()>() != MAGIC)
    throw tsfile_badmagic();

  std::uint32_t version = in.get_uint32();
  major_version = static_cast<std::uint16_t>(version >> 16);
  minor_version = static_cast<std::uint16_t>(version & 0xffu);
}

inline void tsfile_mimeheader::write(monsoon::xdr::xdr_ostream& out) const {
  const std::uint32_t version =
      (static_cast<std::uint32_t>(major_version) << 16) |
      static_cast<std::uint32_t>(minor_version);

  out.put_bytes(MAGIC);
  out.put_uint32(version);
}


}} /* namespace monsoon::history */

#endif /* TSDATA_MIME_INL_H */
