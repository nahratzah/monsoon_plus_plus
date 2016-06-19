#ifndef MONSOON_EXPRESSIONS_OPERATORS_MINUS_H
#define MONSOON_EXPRESSIONS_OPERATORS_MINUS_H

#include <monsoon/expression.h>
#include <monsoon/expressions/operators/binop.h>

namespace monsoon {
namespace expressions {
namespace operators {


class minus final
: public binop
{
 public:
  minus(std::unique_ptr<expression>, std::unique_ptr<expression>,
        std::unique_ptr<match_clause>);
  ~minus() noexcept override;

 protected:
  metric_value evaluate(const metric_value&, const metric_value&)
      const noexcept override;
};


}}} /* namespace monsoon::expressions::operators */

#endif /* MONSOON_EXPRESSIONS_OPERATORS_MINUS_H */
