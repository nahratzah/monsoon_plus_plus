#ifndef MONSOON_METRIC_SOURCE_H
#define MONSOON_METRIC_SOURCE_H

#include <monsoon/intf_export_.h>
#include <monsoon/acceptor.h>
#include <monsoon/group_name.h>
#include <monsoon/metric_name.h>
#include <cstdint>
#include <unordered_set>
#include <unordered_map>
#include <monsoon/time_point.h>
#include <monsoon/metric_value.h>
#include <monsoon/time_range.h>
#include <monsoon/objpipe/reader.h>

namespace monsoon {


class monsoon_intf_export_ metric_source {
 public:
  struct metrics_hash {
    std::size_t monsoon_intf_export_ operator()(
        const std::tuple<group_name, metric_name>&) const noexcept;
    std::size_t monsoon_intf_export_ operator()(
        const std::tuple<simple_group, metric_name>&) const noexcept;
  };

  using speculative_metric_emit =
      std::tuple<time_point, group_name, metric_name, metric_value>;
  using metric_emit = std::tuple<
      time_point,
      std::unordered_map<
          std::tuple<group_name, metric_name>,
          metric_value,
          metrics_hash>>;
  using emit_type = std::variant<speculative_metric_emit, metric_emit>;

  virtual ~metric_source() noexcept;

  virtual auto simple_groups(const time_range&) const
      -> std::unordered_set<simple_group> = 0;
  virtual auto group_names(const time_range&) const
      -> std::unordered_set<group_name> = 0;
  virtual auto tagged_metrics(const time_range&) const
      -> std::unordered_set<std::tuple<group_name, metric_name>, metrics_hash> = 0;
  virtual auto untagged_metrics(const time_range&) const
      -> std::unordered_set<std::tuple<simple_group, metric_name>, metrics_hash> = 0;

  virtual auto emit(
      time_range,
      std::function<bool(const group_name&)>,
      std::function<bool(const group_name&, const metric_name&)>,
      time_point::duration = time_point::duration(0)) const
      -> objpipe::reader<emit_type> = 0;
  virtual auto emit(
      time_range,
      std::function<bool(const simple_group&)>,
      std::function<bool(const simple_group&, const metric_name&)>,
      time_point::duration = time_point::duration(0)) const
      -> objpipe::reader<emit_type>;
  virtual auto emit_time(
      time_range,
      time_point::duration = time_point::duration(0)) const
      -> objpipe::reader<time_point> = 0;
};


} /* namespace monsoon */

#endif /* MONSOON_METRIC_SOURCE_H */
