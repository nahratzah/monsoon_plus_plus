#include "tsdata_mime.h"

namespace monsoon {
namespace history {


constexpr std::array<std::uint8_t, 12> tsfile_mimeheader::MAGIC;

tsfile_badmagic::~tsfile_badmagic() {}


}} /* monsoon::history */
