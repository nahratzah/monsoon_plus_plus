#ifndef V2_TSDATA_LIST_H
#define V2_TSDATA_LIST_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include "encdec.h"
#include "tsdata.h"

namespace monsoon {
namespace history {
namespace v2 {


class monsoon_dirhistory_local_ tsdata_v2_list
: public tsdata_v2
{
 public:
  ~tsdata_v2_list() noexcept override;

  std::shared_ptr<io::fd> fd() const noexcept override;
  std::vector<time_series> read_all() const override;

 private:
  file_segment<tsdata_list> data_;
};


}}} /* namespace monsoon::history::v2 */

#endif /* V2_TSDATA_LIST_H */
