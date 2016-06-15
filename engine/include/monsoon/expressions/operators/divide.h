#ifndef MONSOON_EXPRESSIONS_OPERATORS_DIVIDE_H
#define MONSOON_EXPRESSIONS_OPERATORS_DIVIDE_H

#include <monsoon/expression.h>
#include <monsoon/expressions/operators/binop.h>

namespace monsoon {
namespace expressions {
namespace operators {


class divide final
: public binop
{
 public:
  divide(std::unique_ptr<expression>, std::unique_ptr<expression>);
  ~divide() noexcept override;

 protected:
  metric_value evaluate(const metric_value&, const metric_value&)
      const noexcept override;
};


}}} /* namespace monsoon::expressions::operators */

#endif /* MONSOON_EXPRESSIONS_OPERATORS_DIVIDE_H */
