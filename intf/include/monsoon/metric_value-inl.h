#ifndef MONSOON_METRIC_VALUE_INL_H
#define MONSOON_METRIC_VALUE_INL_H

#include <utility>

namespace monsoon {


inline metric_value::metric_value(metric_value&& other) noexcept
: value_(std::move(other.value_))
{}

inline auto metric_value::operator=(metric_value&& other) noexcept
->  metric_value& {
  value_ = std::move(other.value_);
  return *this;
}

inline metric_value::metric_value(bool v) noexcept
: value_(types::create<0>(v))
{}

inline metric_value::metric_value(signed_type v) noexcept
: value_(v >= 0 ? types::create<2>(v) : types::create<1>(v))
{}

inline metric_value::metric_value(unsigned_type v) noexcept
: value_(types::create<2>(v))
{}

inline metric_value::metric_value(fp_type v) noexcept
: value_(types::create<3>(v))
{}

inline metric_value::metric_value(std::string v) noexcept
: value_(types::create<4>(std::move(v)))
{}

inline metric_value::metric_value(histogram v) noexcept
: value_(types::create<5>(std::move(v)))
{}

inline auto metric_value::operator!=(const metric_value& other) const noexcept
->  bool {
  return !(*this == other);
}

inline auto metric_value::get() const noexcept -> const optional<types>& {
  return value_;
}


} /* namespace monsoon */

#endif /* MONSOON_METRIC_VALUE_INL_H */
