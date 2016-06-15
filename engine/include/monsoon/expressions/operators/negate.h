#ifndef MONSOON_EXPRESSIONS_OPERATORS_NEGATE_H
#define MONSOON_EXPRESSIONS_OPERATORS_NEGATE_H

#include <monsoon/expression.h>
#include <monsoon/expressions/operators/unop.h>

namespace monsoon {
namespace expressions {
namespace operators {


class negate final
: public unop
{
 public:
  negate(std::unique_ptr<expression>);
  ~negate() noexcept override;

 protected:
  metric_value evaluate(const metric_value&) const noexcept override;
};


}}} /* namespace monsoon::expressions::operators */

#endif /* MONSOON_EXPRESSIONS_OPERATORS_NEGATE_H */
