#include <monsoon/expressions/operators/cmp_gt.h>
#include <utility>

namespace monsoon {
namespace expressions {
namespace operators {


cmp_gt::cmp_gt(std::unique_ptr<expression> x, std::unique_ptr<expression> y,
               std::unique_ptr<match_clause> matcher)
: binop(">", std::move(x), std::move(y), std::move(matcher))
{}

cmp_gt::~cmp_gt() noexcept {}

auto cmp_gt::evaluate(const metric_value& x, const metric_value& y)
  const noexcept
->  metric_value {
  return greater(x, y);
}


}}} /* namespace monsoon::expressions::operators */
