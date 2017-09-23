#ifndef MONSOON_TIME_SERIES_INL_H
#define MONSOON_TIME_SERIES_INL_H

#include <utility>

namespace monsoon {


inline time_series::time_series(time_point tp)
: tp_(std::move(tp))
{}

template<typename Iter>
inline time_series::time_series(time_point tp, Iter b, Iter e)
: tp_(std::move(tp)),
  tsvs_(b, e)
{}

inline time_series::time_series(time_point tp, tsv_set tsvs) noexcept
: tp_(std::move(tp)),
  tsvs_(std::move(tsvs))
{}

inline time_series::time_series(time_point tp,
    std::initializer_list<time_series_value> il)
: tp_(std::move(tp)),
  tsvs_(il)
{}

inline auto time_series::get_time() const noexcept -> const time_point& {
  return tp_;
}

inline auto time_series::get_data() const noexcept -> const tsv_set& {
  return tsvs_;
}


} /* namespace monsoon */

#endif /* MONSOON_TIME_SERIES_INL_H */
