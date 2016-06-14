#include <monsoon/expressions/operators/plus.h>
#include <utility>

namespace monsoon {
namespace expressions {
namespace operators {


plus::plus(std::unique_ptr<expression> x, std::unique_ptr<expression> y)
: numeric_binop(" + ", std::move(x), std::move(y))
{}

plus::~plus() noexcept {}

auto plus::evaluate(long x, long y) const
->  metric_value {
  return metric_value(x + y);
}

auto plus::evaluate(long x, unsigned long y) const
->  metric_value {
  return metric_value(x + y);
}

auto plus::evaluate(long x, double y) const
->  metric_value {
  return metric_value(x + y);
}

auto plus::evaluate(unsigned long x, long y) const
->  metric_value {
  return metric_value(x + y);
}

auto plus::evaluate(unsigned long x, unsigned long y) const
->  metric_value {
  return metric_value(x + y);
}

auto plus::evaluate(unsigned long x, double y) const
->  metric_value {
  return metric_value(x + y);
}

auto plus::evaluate(double x, long y) const
->  metric_value {
  return metric_value(x + y);
}

auto plus::evaluate(double x, unsigned long y) const
->  metric_value {
  return metric_value(x + y);
}

auto plus::evaluate(double x, double y) const
->  metric_value {
  return metric_value(x + y);
}


}}} /* namespace monsoon::expressions::operators */
