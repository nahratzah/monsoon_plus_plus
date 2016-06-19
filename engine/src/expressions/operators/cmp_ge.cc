#include <monsoon/expressions/operators/cmp_ge.h>
#include <utility>

namespace monsoon {
namespace expressions {
namespace operators {


cmp_ge::cmp_ge(std::unique_ptr<expression> x, std::unique_ptr<expression> y,
               std::unique_ptr<match_clause> matcher)
: binop(">=", std::move(x), std::move(y), std::move(matcher))
{}

cmp_ge::~cmp_ge() noexcept {}

auto cmp_ge::evaluate(const metric_value& x, const metric_value& y)
  const noexcept
->  metric_value {
  return greater_equal(x, y);
}


}}} /* namespace monsoon::expressions::operators */
