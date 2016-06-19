#include <monsoon/expressions/operators/modulo.h>
#include <utility>

namespace monsoon {
namespace expressions {
namespace operators {


modulo::modulo(std::unique_ptr<expression> x, std::unique_ptr<expression> y,
               std::unique_ptr<match_clause> matcher)
: binop("%", std::move(x), std::move(y), std::move(matcher))
{}

modulo::~modulo() noexcept {}

auto modulo::evaluate(const metric_value& x, const metric_value& y)
  const noexcept
->  metric_value {
  return x % y;
}


}}} /* namespace monsoon::expressions::operators */
