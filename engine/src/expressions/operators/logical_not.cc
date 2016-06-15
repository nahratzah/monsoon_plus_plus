#include <monsoon/expressions/operators/logical_not.h>
#include <utility>

namespace monsoon {
namespace expressions {
namespace operators {


logical_not::logical_not(std::unique_ptr<expression> x)
: unop("!", std::move(x))
{}

logical_not::~logical_not() noexcept {}

auto logical_not::evaluate(const metric_value& x) const noexcept
->  metric_value {
  return !x;
}


}}} /* namespace monsoon::expressions::operators */
