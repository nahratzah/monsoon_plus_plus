#ifndef MONSOON_EXPRESSIONS_OPERATORS_EQUALITY_H
#define MONSOON_EXPRESSIONS_OPERATORS_EQUALITY_H

#include <monsoon/expression.h>
#include <monsoon/expressions/operators/binop.h>

namespace monsoon {
namespace expressions {
namespace operators {


class equality final
: public binop
{
 public:
  equality(std::unique_ptr<expression>, std::unique_ptr<expression>,
           std::unique_ptr<match_clause>);
  ~equality() noexcept override;

 protected:
  metric_value evaluate(const metric_value&, const metric_value&)
      const noexcept override;
};


}}} /* namespace monsoon::expressions::operators */

#endif /* MONSOON_EXPRESSIONS_OPERATORS_EQUALITY_H */
