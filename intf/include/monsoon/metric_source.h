#ifndef MONSOON_METRIC_SOURCE_H
#define MONSOON_METRIC_SOURCE_H

///\file
///\ingroup intf

#include <monsoon/intf_export_.h>
#include <monsoon/group_name.h>
#include <monsoon/metric_name.h>
#include <cstdint>
#include <unordered_set>
#include <unordered_map>
#include <monsoon/time_point.h>
#include <monsoon/metric_value.h>
#include <monsoon/time_range.h>
#include <monsoon/path_matcher.h>
#include <monsoon/tag_matcher.h>
#include <monsoon/objpipe/reader.h>

namespace monsoon {


/**
 * \brief A source of metrics.
 * \ingroup intf
 *
 * A metric source has access to named metrics over time.
 */
class monsoon_intf_export_ metric_source {
 public:
  ///\brief Make it possible to compute the hash of a name.
  struct metrics_hash {
    std::size_t monsoon_intf_export_ operator()(
        const std::tuple<group_name, metric_name>&) const noexcept;
    std::size_t monsoon_intf_export_ operator()(
        const std::tuple<simple_group, metric_name>&) const noexcept;
  };

  /**
   * \brief Speculative emit type.
   *
   * A speculative emit type is done as early as possible,
   * but may be invalidated by later speculative emitions or
   * by \ref metric_emit "factual emitions".
   *
   * \todo Rename to speculative_emit_type, for consistency with \ref monsoon::expression.
   */
  using speculative_metric_emit =
      std::tuple<time_point, group_name, metric_name, metric_value>;
  /**
   * \brief Factual emit type.
   *
   * A factual emit contains all emited values for the given timestamp.
   * Once a factual emition has been emitted, no more emitions at/before its
   * timestamp will happen.
   *
   * \todo Rename to factual_emit_type, for consistency with \ref monsoon::expression.
   */
  using metric_emit = std::tuple<
      time_point,
      std::unordered_map<
          std::tuple<group_name, metric_name>,
          metric_value,
          metrics_hash>>;
  /**
   * \brief Emition type.
   *
   * Describes the values emited by \ref objpipe::reader returned by the emit function.
   */
  using emit_type = std::variant<speculative_metric_emit, metric_emit>;

  ///\brief Metric source is virtual.
  virtual ~metric_source() noexcept;

  ///@{
  /**
   * \brief Retrieve all metrics matching the given filters over time.
   *
   * \param tr The interval over which to yield metrics.
   * \param group_filter A predicate on group.
   * \param metric_filter A predicate on metrics. Only invoked if the group passes the group filter predicate.
   * \param slack Extra time before and after the time range, to fill in interpolated values.
   * \return An \ref objpipe::reader emitting the \ref emit_type.
   */
  virtual auto emit(
      time_range tr,
      path_matcher group_filter,
      tag_matcher group_tag_filter,
      path_matcher metric_filter,
      time_point::duration slack = time_point::duration(0)) const
      -> objpipe::reader<emit_type> = 0;
  /**
   * \brief Retrieve all time points over time.
   *
   * \param tr The interval over which to yield time points.
   * \param slack Extra time before and after the time range, to fill in interpolated values.
   * \return An \ref objpipe::reader emitting the \ref emit_type.
   */
  virtual auto emit_time(
      time_range,
      time_point::duration = time_point::duration(0)) const
      -> objpipe::reader<time_point> = 0;
  ///@}
};


} /* namespace monsoon */

#endif /* MONSOON_METRIC_SOURCE_H */
