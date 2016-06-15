#include <monsoon/expressions/operators/logical_or.h>
#include <utility>

namespace monsoon {
namespace expressions {
namespace operators {


logical_or::logical_or(std::unique_ptr<expression> x,
                       std::unique_ptr<expression> y)
: binop(" || ", std::move(x), std::move(y))
{}

logical_or::~logical_or() noexcept {}

auto logical_or::evaluate(const metric_value& x, const metric_value& y)
  const noexcept
->  metric_value {
  return x || y;
}


}}} /* namespace monsoon::expressions::operators */
