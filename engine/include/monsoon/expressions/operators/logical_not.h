#ifndef MONSOON_EXPRESSIONS_OPERATORS_LOGICAL_NOT_H
#define MONSOON_EXPRESSIONS_OPERATORS_LOGICAL_NOT_H

#include <monsoon/expression.h>
#include <monsoon/expressions/operators/unop.h>

namespace monsoon {
namespace expressions {
namespace operators {


class logical_not final
: public unop
{
 public:
  logical_not(std::unique_ptr<expression>);
  ~logical_not() noexcept override;

 protected:
  metric_value evaluate(const metric_value&) const noexcept override;
};


}}} /* namespace monsoon::expressions::operators */

#endif /* MONSOON_EXPRESSIONS_OPERATORS_LOGICAL_NOT_H */
