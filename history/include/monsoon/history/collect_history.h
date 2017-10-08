#ifndef MONSOON_HISTORY_COLLECT_HISTORY_H
#define MONSOON_HISTORY_COLLECT_HISTORY_H

#include <monsoon/history/history_export_.h>
#include <monsoon/time_series.h>
#include <monsoon/time_range.h>
#include <monsoon/simple_group.h>
#include <monsoon/group_name.h>
#include <monsoon/metric_name.h>
#include <unordered_set>

namespace monsoon {


class monsoon_history_export_ collect_history {
 public:
  collect_history() noexcept = default;
  virtual ~collect_history() noexcept;

  virtual void push_back(const time_series&) = 0;

  virtual auto simple_groups(const time_range&) const
      -> std::unordered_set<simple_group> = 0;
  virtual auto group_names(const time_range&) const
      -> std::unordered_set<group_name> = 0;
  virtual auto tagged_metrics(const time_range&) const
      -> std::unordered_multimap<group_name, metric_name> = 0;
  virtual auto untagged_metrics(const time_range&) const
      -> std::unordered_multimap<simple_group, metric_name> = 0;

 protected:
  collect_history(const collect_history&) noexcept = default;
  collect_history(collect_history&&) noexcept = default;
};


} /* namespace monsoon */

#endif /* MONSOON_HISTORY_COLLECT_HISTORY_H */
