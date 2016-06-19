#ifndef MONSOON_EXPRESSIONS_OPERATORS_CMP_GE_H
#define MONSOON_EXPRESSIONS_OPERATORS_CMP_GE_H

#include <monsoon/expression.h>
#include <monsoon/expressions/operators/binop.h>

namespace monsoon {
namespace expressions {
namespace operators {


class cmp_ge final
: public binop
{
 public:
  cmp_ge(std::unique_ptr<expression>, std::unique_ptr<expression>,
         std::unique_ptr<match_clause>);
  ~cmp_ge() noexcept override;

 protected:
  metric_value evaluate(const metric_value&, const metric_value&)
      const noexcept override;
};


}}} /* namespace monsoon::expressions::operators */

#endif /* MONSOON_EXPRESSIONS_OPERATORS_CMP_GE_H */
