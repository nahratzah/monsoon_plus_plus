#include <monsoon/expressions/operators/divide.h>
#include <utility>

namespace monsoon {
namespace expressions {
namespace operators {


divide::divide(std::unique_ptr<expression> x, std::unique_ptr<expression> y)
: binop(" / ", std::move(x), std::move(y))
{}

divide::~divide() noexcept {}

auto divide::evaluate(const metric_value& x, const metric_value& y)
  const noexcept
->  metric_value {
  return x / y;
}


}}} /* namespace monsoon::expressions::operators */
