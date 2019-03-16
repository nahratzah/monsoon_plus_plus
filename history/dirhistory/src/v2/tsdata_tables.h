#ifndef V2_TSDATA_TABLES_H
#define V2_TSDATA_TABLES_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include "encdec.h"
#include "tsdata.h"
#include "../tsdata_mime.h"
#include "tsfile_header.h"
#include <monsoon/io/fd.h>
#include <memory>

namespace monsoon {
namespace history {
namespace v2 {


class monsoon_dirhistory_local_ tsdata_v2_tables
: public tsdata_v2,
  public std::enable_shared_from_this<tsdata_v2_tables>
{
 public:
  tsdata_v2_tables(io::fd&& fd, const tsfile_mimeheader& mime, const tsfile_header& hdr)
  : tsdata_v2(std::move(fd), mime, hdr)
  {}

  ~tsdata_v2_tables() noexcept override;

  bool is_writable() const noexcept override;
  void push_back(const emit_type&) override;

  auto emit(
      std::optional<time_point>,
      std::optional<time_point>,
      const path_matcher&,
      const tag_matcher&,
      const path_matcher&) const
  -> objpipe::reader<emit_type> override;
  auto emit_time(
      std::optional<time_point>,
      std::optional<time_point>) const
  -> objpipe::reader<time_point> override;

 private:
  auto read_() const -> std::shared_ptr<file_data_tables>;

  std::vector<time_series> read_all_raw_() const override;
};


}}} /* namespace monsoon::history::v2 */

#endif /* V2_TSDATA_TABLES_H */
