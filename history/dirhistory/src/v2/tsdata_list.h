#ifndef V2_TSDATA_LIST_H
#define V2_TSDATA_LIST_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <monsoon/xdr/xdr.h>
#include <monsoon/time_point.h>
#include "encdec.h"
#include "tsdata.h"

namespace monsoon {
namespace history {
namespace v2 {


class monsoon_dirhistory_local_ tsdata_v2_list
: public tsdata_v2
{
 public:
  tsdata_v2_list(file_segment<tsdata_list>&&, const tsdata_v2::carg&);
  ~tsdata_v2_list() noexcept override;

  std::shared_ptr<io::fd> fd() const noexcept override;
  bool is_writable() const noexcept override;
  void push_back(const time_series&) override;

  std::unordered_set<simple_group> simple_groups() const override;
  std::unordered_set<group_name> group_names() const override;
  std::unordered_multimap<simple_group, metric_name> untagged_metrics() const
      override;
  std::unordered_multimap<group_name, metric_name> tagged_metrics() const
      override;

 private:
  std::vector<time_series> read_all_raw_() const override;

  template<typename Fn>
  void visit(Fn) const;

  file_segment<tsdata_list> data_;
};


}}} /* namespace monsoon::history::v2 */

#include "tsdata_list-inl.h"

#endif /* V2_TSDATA_LIST_H */
