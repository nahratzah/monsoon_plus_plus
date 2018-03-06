#ifndef MONSOON_METRIC_VALUE_INL_H
#define MONSOON_METRIC_VALUE_INL_H

#include <utility>
#include <monsoon/overload.h>

namespace monsoon {


inline metric_value::metric_value(bool v) noexcept
: value_(std::in_place_type<bool>, v)
{}

inline metric_value::metric_value(fp_type v) noexcept
: value_(std::in_place_type<fp_type>, v)
{}

inline metric_value::metric_value(const char* v) noexcept
: metric_value(std::string_view(v))
{}

inline metric_value::metric_value(histogram v) noexcept
: value_(std::in_place_type<histogram>, std::move(v))
{}

template<typename T, typename /* enable_if_t */>
metric_value::metric_value(const T& v) noexcept
: value_(v >= T(0)
    ? internal_types(std::in_place_type<unsigned_type>, v)
    : internal_types(std::in_place_type<signed_type>, v))
{}

inline auto metric_value::operator!=(const metric_value& other) const noexcept
->  bool {
  return !(*this == other);
}

inline auto metric_value::get() const noexcept -> types {
  return std::visit(
      overload(
          [](const auto& v) -> types { return v; },
          [](const string_ptr& p) -> types { return std::string_view(*p); }),
      value_);
}


} /* namespace monsoon */

#endif /* MONSOON_METRIC_VALUE_INL_H */
