#ifndef MONSOON_EXPRESSIONS_OPERATORS_UNEQUALITY_H
#define MONSOON_EXPRESSIONS_OPERATORS_UNEQUALITY_H

#include <monsoon/expression.h>
#include <monsoon/expressions/operators/binop.h>

namespace monsoon {
namespace expressions {
namespace operators {


class inequality final
: public binop
{
 public:
  inequality(std::unique_ptr<expression>, std::unique_ptr<expression>,
             std::unique_ptr<match_clause>);
  ~inequality() noexcept override;

 protected:
  metric_value evaluate(const metric_value&, const metric_value&)
      const noexcept override;
};


}}} /* namespace monsoon::expressions::operators */

#endif /* MONSOON_EXPRESSIONS_OPERATORS_UNEQUALITY_H */
