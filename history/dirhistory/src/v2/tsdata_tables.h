#ifndef V2_TSDATA_TABLES_H
#define V2_TSDATA_TABLES_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include "encdec.h"
#include "tsdata.h"
#include "../tsdata_mime.h"

namespace monsoon {
namespace history {
namespace v2 {


class monsoon_dirhistory_local_ tsdata_v2_tables
: public tsdata_v2
{
 public:
  tsdata_v2_tables(file_segment<file_data_tables>&&, const tsdata_v2::carg&);
  ~tsdata_v2_tables() noexcept override;

  std::shared_ptr<io::fd> fd() const noexcept override;

 private:
  std::vector<time_series> read_all_raw_() const override;

  file_segment<file_data_tables> data_;
};


}}} /* namespace monsoon::history::v2 */

#endif /* V2_TSDATA_TABLES_H */