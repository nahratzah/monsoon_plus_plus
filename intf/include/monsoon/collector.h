#ifndef MONSOON_COLLECTOR_H
#define MONSOON_COLLECTOR_H

#include <monsoon/group_name.h>
#include <monsoon/metric_name.h>
#include <monsoon/metric_value.h>
#include <unordered_map>

namespace monsoon {


class collector {
 public:
  using metric_map = std::unordered_map<metric_name, metric_value>;
  using group_map = std::unordered_map<group_name, metric_map>;

  collector();
  virtual ~collector() noexcept;

  virtual group_map collect() = 0;
};


} /* namespace monsoon */

#endif /* MONSOON_COLLECTOR_H */
