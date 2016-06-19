#include <monsoon/expressions/operators/inequality.h>
#include <utility>

namespace monsoon {
namespace expressions {
namespace operators {


inequality::inequality(std::unique_ptr<expression> x,
                       std::unique_ptr<expression> y,
                       std::unique_ptr<match_clause> matcher)
: binop("=", std::move(x), std::move(y), std::move(matcher))
{}

inequality::~inequality() noexcept {}

auto inequality::evaluate(const metric_value& x, const metric_value& y)
  const noexcept
->  metric_value {
  return equal(x, y);
}


}}} /* namespace monsoon::expressions::operators */
