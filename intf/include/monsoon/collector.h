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
#include <unordered_set>
#include <vector>

namespace monsoon {


/**
 * \brief Collector interface.
 * \ingroup intf
 * \details A collector collects metrics.
 */
class monsoon_intf_export_ collector {
 public:
  using known_names_set = std::unordered_set<std::tuple<group_name, metric_name>>;
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
  virtual auto run(objpipe::reader<time_point>) -> objpipe::reader<collection> = 0;
};


} /* namespace monsoon */

#endif /* MONSOON_COLLECTOR_H */
