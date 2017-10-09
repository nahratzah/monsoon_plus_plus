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
  bool is_writable() const noexcept override;
  void push_back(const time_series&) override;

  std::unordered_set<simple_group> simple_groups() const override;
  std::unordered_set<group_name> group_names() const override;
  std::unordered_set<std::tuple<simple_group, metric_name>, metrics_hash>
      untagged_metrics() const override;
  std::unordered_set<std::tuple<group_name, metric_name>, metrics_hash>
      tagged_metrics() const override;

  void emit(
      emit_acceptor<group_name, metric_name, metric_value>&,
      std::optional<time_point>,
      std::optional<time_point>,
      const std::unordered_multimap<group_name, metric_name>&) const override;
  void emit(
      emit_acceptor<group_name, metric_name, metric_value>&,
      std::optional<time_point>,
      std::optional<time_point>,
      const std::unordered_multimap<simple_group, metric_name>&) const override;

 private:
  std::vector<time_series> read_all_raw_() const override;

  file_segment<file_data_tables> data_;
};


}}} /* namespace monsoon::history::v2 */

#endif /* V2_TSDATA_TABLES_H */
