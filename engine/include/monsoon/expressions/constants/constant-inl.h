#ifndef MONSOON_EXPRESSIONS_CONSTANT_INL_H
#define MONSOON_EXPRESSIONS_CONSTANT_INL_H

namespace monsoon {
namespace expressions {
namespace constants {


inline constant::constant(bool v) noexcept
: constant(metric_value(v))
{}

inline constant::constant(metric_value::signed_type v) noexcept
: constant(metric_value(v))
{}

inline constant::constant(metric_value::unsigned_type v) noexcept
: constant(metric_value(v))
{}

inline constant::constant(metric_value::fp_type v) noexcept
: constant(metric_value(v))
{}

inline constant::constant(std::string v) noexcept
: constant(metric_value(std::move(v)))
{}

inline constant::constant(histogram v) noexcept
: constant(metric_value(std::move(v)))
{}



}}} /* namespace monsoon::expressions::constants */

#endif /* MONSOON_EXPRESSIONS_CONSTANT_INL_H */
