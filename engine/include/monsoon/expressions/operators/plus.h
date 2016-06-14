#ifndef MONSOON_EXPRESSIONS_OPERATORS_PLUS_H
#define MONSOON_EXPRESSIONS_OPERATORS_PLUS_H

#include <monsoon/expression.h>
#include <monsoon/expressions/operators/numeric_binop.h>

namespace monsoon {
namespace expressions {
namespace operators {


class plus final
: public numeric_binop
{
 public:
  plus(std::unique_ptr<expression>, std::unique_ptr<expression>);
  ~plus() noexcept override;

 protected:
  metric_value evaluate(long, long) const override;
  metric_value evaluate(long, unsigned long) const override;
  metric_value evaluate(long, double) const override;
  metric_value evaluate(unsigned long, long) const override;
  metric_value evaluate(unsigned long, unsigned long) const override;
  metric_value evaluate(unsigned long, double) const override;
  metric_value evaluate(double, long) const override;
  metric_value evaluate(double, unsigned long) const override;
  metric_value evaluate(double, double) const override;
};


}}} /* namespace monsoon::expressions::operators */

#endif /* MONSOON_EXPRESSIONS_OPERATORS_PLUS_H */
