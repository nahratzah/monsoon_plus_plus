#include <monsoon/expressions/operators/negate.h>
#include <utility>

namespace monsoon {
namespace expressions {
namespace operators {


negate::negate(std::unique_ptr<expression> x)
: unop("-", std::move(x))
{}

negate::~negate() noexcept {}

auto negate::evaluate(const metric_value& x) const noexcept
->  metric_value {
  return -x;
}


}}} /* namespace monsoon::expressions::operators */
