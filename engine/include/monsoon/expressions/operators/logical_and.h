#ifndef MONSOON_EXPRESSIONS_OPERATORS_LOGICAL_AND_H
#define MONSOON_EXPRESSIONS_OPERATORS_LOGICAL_AND_H

#include <monsoon/expression.h>
#include <monsoon/expressions/operators/binop.h>

namespace monsoon {
namespace expressions {
namespace operators {


class logical_and final
: public binop
{
 public:
  logical_and(std::unique_ptr<expression>, std::unique_ptr<expression>,
              std::unique_ptr<match_clause>);
  ~logical_and() noexcept override;

 protected:
  metric_value evaluate(const metric_value&, const metric_value&)
      const noexcept override;
};


}}} /* namespace monsoon::expressions::operators */

#endif /* MONSOON_EXPRESSIONS_OPERATORS_LOGICAL_AND_H */
