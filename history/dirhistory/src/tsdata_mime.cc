#include "tsdata_mime.h"

namespace monsoon {
namespace history {


const std::array<std::uint8_t, 12> tsfile_mimeheader::MAGIC = {{
  17u, 19u, 23u, 29u,
  'M', 'O', 'N', '-',
  's', 'o', 'o', 'n'
}};

tsfile_badmagic::~tsfile_badmagic() {}


}} /* monsoon::history */
