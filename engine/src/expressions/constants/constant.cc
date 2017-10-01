#include <monsoon/expressions/constants/constant.h>
#include <stdexcept>

namespace monsoon {
namespace expressions {
namespace constants {


constant::constant(metric_value v)
: value_(std::move(v))
{
  if (visit(
          [](const auto& v) {
            return std::is_same_v<metric_value::empty, std::decay_t<decltype(v)>>;
          },
          v.get()))
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
