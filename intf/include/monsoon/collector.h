#ifndef MONSOON_COLLECTOR_H
#define MONSOON_COLLECTOR_H

///\file
///\ingroup intf

#include <monsoon/group_name.h>
#include <monsoon/intf_export_.h>
#include <monsoon/metric_name.h>
#include <monsoon/metric_value.h>
#include <monsoon/path_matcher.h>
#include <monsoon/tag_matcher.h>
#include <monsoon/time_point.h>
#include <objpipe/reader.h>
#include <tuple>
#include <functional>
#include <memory>
#include <set>
#include <vector>

namespace monsoon {


/**
 * \brief Collector interface.
 * \ingroup intf
 * \details A collector collects metrics.
 */
class monsoon_intf_export_ collector {
 public:
  using known_names_set = std::set<std::tuple<group_name, metric_name>>;
  using unknown_names_set = std::vector<std::tuple<path_matcher, tag_matcher, path_matcher>>;

  struct names_set {
    known_names_set known;
    unknown_names_set unknown;
  };

  struct collection_element {
    group_name group;
    metric_name metric;
    metric_value value;
  };

  struct collection {
    time_point tp;
    std::vector<collection_element> elements;
    bool is_complete;
  };

  virtual ~collector() noexcept;

  /**
   * \brief Set of names provided by this collector.
   * \details
   * The collector may not emit names that don't match the returned constraint.
   */
  virtual auto provides() const -> names_set = 0;

  /**
   * \brief Create a run instance of the collector.
   * \details
   * The collector performs a permutation on a time point,
   * yielding the metrics at that time point.
   *
   * The yielded metrics must match the constraint returned by provides().
   */
  virtual auto run(objpipe::reader<time_point> tp_pipe) -> objpipe::reader<collection> = 0;
};


/**
 * \brief Synchronous collector.
 * \ingroup intf
 * \details
 * This is a collector that retrieves metrics immediately.
 *
 * Immediately means there's no waiting involved, like for instance
 * it does not require a network connection.
 *
 * Mainly used for system metrics and internal metrics.
 */
class monsoon_intf_export_ sync_collector
: public collector,
  public std::enable_shared_from_this<sync_collector>
{
 public:
  sync_collector(known_names_set names) noexcept
  : names_(std::move(names))
  {}

  ~sync_collector() noexcept override;

  auto provides() const -> names_set override final;
  auto run(objpipe::reader<time_point> tp_pipe) -> objpipe::reader<collection> override final;

 private:
  virtual auto do_collect() -> std::vector<collection_element> = 0;

  known_names_set names_;
};


} /* namespace monsoon */

#endif /* MONSOON_COLLECTOR_H */
