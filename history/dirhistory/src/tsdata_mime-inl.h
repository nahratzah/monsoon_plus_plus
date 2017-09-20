#ifndef TSDATA_MIME_INL_H
#define TSDATA_MIME_INL_H

namespace monsoon {
namespace history {


inline tsfile_mimeheader::tsfile_mimeheader(std::uint16_t majver,
    std::uint16_t minver) noexcept
: major_version(majver),
  minor_version(minver)
{}

inline tsfile_mimeheader::tsfile_mimeheader(monsoon::xdr::xdr_istream& in)
: tsfile_mimeheader(read(in).template release_or_throw<tsfile_badmagic>())
{}

inline auto tsfile_mimeheader::read(monsoon::xdr::xdr_istream& in)
-> optional<tsfile_mimeheader> {
  if (in.template get_array<MAGIC.size()>() != MAGIC)
    return {};

  std::uint32_t version = in.get_uint32();
  return tsfile_mimeheader(
      static_cast<std::uint16_t>(version >> 16),
      static_cast<std::uint16_t>(version & 0xffu));
}

inline void tsfile_mimeheader::write(monsoon::xdr::xdr_ostream& out) const {
  const std::uint32_t version =
      (static_cast<std::uint32_t>(major_version) << 16) |
      static_cast<std::uint32_t>(minor_version);

  out.put_array(MAGIC);
  out.put_uint32(version);
}


}} /* namespace monsoon::history */

#endif /* TSDATA_MIME_INL_H */
