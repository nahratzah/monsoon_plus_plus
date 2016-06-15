#include <monsoon/expressions/operators/minus.h>
#include <utility>

namespace monsoon {
namespace expressions {
namespace operators {


minus::minus(std::unique_ptr<expression> x, std::unique_ptr<expression> y)
: binop(" - ", std::move(x), std::move(y))
{}

minus::~minus() noexcept {}

auto minus::evaluate(const metric_value& x, const metric_value& y)
  const noexcept
->  metric_value {
  return x - y;
}


}}} /* namespace monsoon::expressions::operators */
