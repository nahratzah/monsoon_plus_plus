#include <monsoon/expressions/operators/cmp_le.h>
#include <utility>

namespace monsoon {
namespace expressions {
namespace operators {


cmp_le::cmp_le(std::unique_ptr<expression> x, std::unique_ptr<expression> y,
               std::unique_ptr<match_clause> matcher)
: binop("<=", std::move(x), std::move(y), std::move(matcher))
{}

cmp_le::~cmp_le() noexcept {}

auto cmp_le::evaluate(const metric_value& x, const metric_value& y)
  const noexcept
->  metric_value {
  return less_equal(x, y);
}


}}} /* namespace monsoon::expressions::operators */
