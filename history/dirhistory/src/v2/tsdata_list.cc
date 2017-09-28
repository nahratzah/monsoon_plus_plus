#include "tsdata_list.h"

namespace monsoon {
namespace history {
namespace v2 {


tsdata_v2_list::~tsdata_v2_list() noexcept {}

std::shared_ptr<io::fd> tsdata_v2_list::fd() const noexcept {
  return data_.ctx().fd();
}


}}} /* namespace monsoon::history::v2 */
