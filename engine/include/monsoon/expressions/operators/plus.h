#ifndef MONSOON_EXPRESSIONS_OPERATORS_PLUS_H
#define MONSOON_EXPRESSIONS_OPERATORS_PLUS_H

#include <monsoon/expression.h>
#include <monsoon/expressions/operators/binop.h>

namespace monsoon {
namespace expressions {
namespace operators {


class plus final
: public binop
{
 public:
  plus(std::unique_ptr<expression>, std::unique_ptr<expression>,
       std::unique_ptr<match_clause>);
  ~plus() noexcept override;

 protected:
  metric_value evaluate(const metric_value&, const metric_value&)
      const noexcept override;
};


}}} /* namespace monsoon::expressions::operators */

#endif /* MONSOON_EXPRESSIONS_OPERATORS_PLUS_H */
