#ifndef V2_TSDATA_TABLES_H
#define V2_TSDATA_TABLES_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include "encdec.h"
#include "tsdata.h"

namespace monsoon {
namespace history {
namespace v2 {


class monsoon_dirhistory_local_ tsdata_v2_tables
: public tsdata_v2
{
 public:
  ~tsdata_v2_tables() noexcept override;

  std::shared_ptr<io::fd> fd() const noexcept override;
  std::vector<time_series> read_all() const override;

 private:
  file_segment<file_data_tables> data_;
};


}}} /* namespace monsoon::history::v2 */

#endif /* V2_TSDATA_TABLES_H */
