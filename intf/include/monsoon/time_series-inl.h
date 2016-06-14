#ifndef MONSOON_TIME_SERIES_INL_H
#define MONSOON_TIME_SERIES_INL_H

#include <utility>

namespace monsoon {


inline time_series::time_series(time_point tp)
: tp_(std::move(tp))
{}

template<typename Iter>
time_series::time_series(time_point tp, Iter b, Iter e)
: tp_(std::move(tp)),
  tsvs_(b, e)
{}

inline auto time_series::get_time() const noexcept -> const time_point& {
  return tp_;
}

inline auto time_series::get_data() const noexcept -> const tsv_set& {
  return tsvs_;
}


} /* namespace monsoon */

#endif /* MONSOON_TIME_SERIES_INL_H */
