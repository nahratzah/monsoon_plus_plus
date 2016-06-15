#include <monsoon/expressions/operators/multiply.h>
#include <utility>

namespace monsoon {
namespace expressions {
namespace operators {


multiply::multiply(std::unique_ptr<expression> x,
                   std::unique_ptr<expression> y)
: binop(" * ", std::move(x), std::move(y))
{}

multiply::~multiply() noexcept {}

auto multiply::evaluate(const metric_value& x, const metric_value& y)
  const noexcept
->  metric_value {
  return x * y;
}


}}} /* namespace monsoon::expressions::operators */
