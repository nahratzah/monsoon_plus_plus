#include <monsoon/expressions/operators/cmp_lt.h>
#include <utility>

namespace monsoon {
namespace expressions {
namespace operators {


cmp_lt::cmp_lt(std::unique_ptr<expression> x, std::unique_ptr<expression> y,
               std::unique_ptr<match_clause> matcher)
: binop("<", std::move(x), std::move(y), std::move(matcher))
{}

cmp_lt::~cmp_lt() noexcept {}

auto cmp_lt::evaluate(const metric_value& x, const metric_value& y)
  const noexcept
->  metric_value {
  return less(x, y);
}


}}} /* namespace monsoon::expressions::operators */
