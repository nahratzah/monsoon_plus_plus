#ifndef MONSOON_TIME_SERIES_H
#define MONSOON_TIME_SERIES_H

#include <monsoon/intf_export_.h>
#include <monsoon/time_point.h>
#include <monsoon/time_series_value.h>
#include <unordered_set>
#include <initializer_list>

namespace monsoon {


class monsoon_intf_export_ time_series {
 public:
  using tsv_set = std::unordered_set<time_series_value>;

  time_series() = default;
  time_series(time_point);
  template<typename Iter> time_series(time_point, Iter, Iter);
  time_series(time_point, tsv_set) noexcept;
  time_series(time_point, std::initializer_list<time_series_value>);

  const time_point& get_time() const noexcept;
  const tsv_set& get_data() const noexcept;

 private:
  time_point tp_;
  tsv_set tsvs_;
};


} /* namespace monsoon */

#include "time_series-inl.h"

#endif /* MONSOON_TIME_SERIES_H */
