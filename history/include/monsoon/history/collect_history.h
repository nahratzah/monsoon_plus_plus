#ifndef MONSOON_HISTORY_COLLECT_HISTORY_H
#define MONSOON_HISTORY_COLLECT_HISTORY_H

#include <monsoon/history/history_export_.h>
#include <monsoon/time_series.h>
#include <monsoon/time_range.h>
#include <monsoon/simple_group.h>
#include <monsoon/group_name.h>
#include <monsoon/metric_name.h>
#include <monsoon/metric_source.h>
#include <unordered_set>

namespace monsoon {


class monsoon_history_export_ collect_history
: public metric_source
{
 public:
  collect_history() noexcept = default;
  virtual ~collect_history() noexcept;

  virtual void push_back(const time_series&) = 0;

  virtual auto time() const -> std::tuple<time_point, time_point> = 0;

 protected:
  collect_history(const collect_history&) noexcept = default;
  collect_history(collect_history&&) noexcept = default;
};


} /* namespace monsoon */

#endif /* MONSOON_HISTORY_COLLECT_HISTORY_H */
