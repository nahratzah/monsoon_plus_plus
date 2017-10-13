#ifndef MONSOON_METRIC_SOURCE_H
#define MONSOON_METRIC_SOURCE_H

#include <monsoon/intf_export_.h>
#include <monsoon/acceptor.h>
#include <monsoon/group_name.h>
#include <monsoon/metric_name.h>
#include <cstdint>
#include <unordered_set>
#include <monsoon/time_point.h>
#include <monsoon/metric_value.h>
#include <monsoon/time_range.h>

namespace monsoon {


class monsoon_intf_export_ metric_source {
 public:
  struct metrics_hash {
    std::size_t monsoon_intf_export_ operator()(
        const std::tuple<group_name, metric_name>&) const noexcept;
    std::size_t monsoon_intf_export_ operator()(
        const std::tuple<simple_group, metric_name>&) const noexcept;
  };

  virtual ~metric_source() noexcept;

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
};


} /* namespace monsoon */

#endif /* MONSOON_METRIC_SOURCE_H */
