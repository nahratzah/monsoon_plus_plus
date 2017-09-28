#include "tsdata_tables.h"

namespace monsoon {
namespace history {
namespace v2 {


tsdata_v2_tables::~tsdata_v2_tables() noexcept {}

std::shared_ptr<io::fd> tsdata_v2_tables::fd() const noexcept {
  return data_.ctx().fd();
}


}}} /* namespace monsoon::history::v2 */
