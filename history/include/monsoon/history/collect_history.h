#ifndef MONSOON_HISTORY_COLLECT_HISTORY_H
#define MONSOON_HISTORY_COLLECT_HISTORY_H

#include <monsoon/history/history_export_.h>
#include <monsoon/time_series.h>
#include <monsoon/time_range.h>
#include <monsoon/simple_group.h>
#include <monsoon/group_name.h>
#include <monsoon/metric_name.h>
#include <monsoon/acceptor.h>
#include <unordered_set>

namespace monsoon {


class monsoon_history_export_ collect_history {
 public:
  struct metrics_hash {
    std::size_t monsoon_history_export_ operator()(
        const std::tuple<group_name, metric_name>&) const noexcept;
    std::size_t monsoon_history_export_ operator()(
        const std::tuple<simple_group, metric_name>&) const noexcept;
  };

  collect_history() noexcept = default;
  virtual ~collect_history() noexcept;

  virtual void push_back(const time_series&) = 0;

  virtual auto time() const -> std::tuple<time_point, time_point> = 0;
  virtual auto simple_groups(const time_range&) const
      -> std::unordered_set<simple_group> = 0;
  virtual auto group_names(const time_range&) const
      -> std::unordered_set<group_name> = 0;
  virtual auto tagged_metrics(const time_range&) const
      -> std::unordered_set<std::tuple<group_name, metric_name>, metrics_hash> = 0;
  virtual auto untagged_metrics(const time_range&) const
      -> std::unordered_set<std::tuple<simple_group, metric_name>, metrics_hash> = 0;

  virtual void emit(
      acceptor<group_name, metric_name, metric_value>&,
      time_range,
      std::unordered_set<std::tuple<group_name, metric_name>, metrics_hash>,
      time_point::duration = time_point::duration(0)) const = 0;
  virtual void emit(
      acceptor<group_name, metric_name, metric_value>&,
      time_range,
      std::unordered_set<std::tuple<simple_group, metric_name>, metrics_hash>,
      time_point::duration = time_point::duration(0)) const = 0;

 protected:
  collect_history(const collect_history&) noexcept = default;
  collect_history(collect_history&&) noexcept = default;
};


} /* namespace monsoon */

#endif /* MONSOON_HISTORY_COLLECT_HISTORY_H */
