#include <monsoon/expressions/operators/logical_and.h>
#include <utility>

namespace monsoon {
namespace expressions {
namespace operators {


logical_and::logical_and(std::unique_ptr<expression> x,
                         std::unique_ptr<expression> y)
: binop(" && ", std::move(x), std::move(y))
{}

logical_and::~logical_and() noexcept {}

auto logical_and::evaluate(const metric_value& x, const metric_value& y)
  const noexcept
->  metric_value {
  return x && y;
}


}}} /* namespace monsoon::expressions::operators */
