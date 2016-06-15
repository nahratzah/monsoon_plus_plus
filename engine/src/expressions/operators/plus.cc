#include <monsoon/expressions/operators/plus.h>
#include <utility>

namespace monsoon {
namespace expressions {
namespace operators {


plus::plus(std::unique_ptr<expression> x, std::unique_ptr<expression> y)
: binop(" + ", std::move(x), std::move(y))
{}

plus::~plus() noexcept {}

auto plus::evaluate(const metric_value& x, const metric_value& y)
  const noexcept
->  metric_value {
  return x + y;
}


}}} /* namespace monsoon::expressions::operators */
