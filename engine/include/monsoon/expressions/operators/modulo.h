#ifndef MONSOON_EXPRESSIONS_OPERATORS_MODULO_H
#define MONSOON_EXPRESSIONS_OPERATORS_MODULO_H

#include <monsoon/expression.h>
#include <monsoon/expressions/operators/binop.h>

namespace monsoon {
namespace expressions {
namespace operators {


class modulo final
: public binop
{
 public:
  modulo(std::unique_ptr<expression>, std::unique_ptr<expression>);
  ~modulo() noexcept override;

 protected:
  metric_value evaluate(const metric_value&, const metric_value&)
      const noexcept override;
};


}}} /* namespace monsoon::expressions::operators */

#endif /* MONSOON_EXPRESSIONS_OPERATORS_MODULO_H */
