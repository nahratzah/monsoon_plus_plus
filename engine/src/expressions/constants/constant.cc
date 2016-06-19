#include <monsoon/expressions/constants/constant.h>
#include <stdexcept>

namespace monsoon {
namespace expressions {
namespace constants {


constant::constant(metric_value v)
: value_(std::move(v))
{
  if (!v.get().is_present())
    throw std::invalid_argument("metric_value may not be nil");
}

constant::~constant() noexcept {}

auto constant::evaluate(const context&) const -> expr_result {
  return expr_result(value_);
}

auto constant::do_ostream(std::ostream& out) const -> void {
  out << value_;
}


}}} /* namespace monsoon::expressions::constants */
