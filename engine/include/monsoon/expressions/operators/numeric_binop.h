#ifndef MONSOON_EXPRESSIONS_OPERATORS_NUMERIC_BINOP_H
#define MONSOON_EXPRESSIONS_OPERATORS_NUMERIC_BINOP_H

#include <monsoon/expressions/operators/binop.h>
#include <monsoon/metric_value.h>

namespace monsoon {
namespace expressions {
namespace operators {


class numeric_binop
: public binop
{
 public:
  using binop::binop;
  ~numeric_binop() noexcept override;

 protected:
  metric_value evaluate(const metric_value&, const metric_value&) const final;
  virtual metric_value evaluate(long, long) const = 0;
  virtual metric_value evaluate(long, unsigned long) const = 0;
  virtual metric_value evaluate(long, double) const = 0;
  virtual metric_value evaluate(unsigned long, long) const = 0;
  virtual metric_value evaluate(unsigned long, unsigned long) const = 0;
  virtual metric_value evaluate(unsigned long, double) const = 0;
  virtual metric_value evaluate(double, long) const = 0;
  virtual metric_value evaluate(double, unsigned long) const = 0;
  virtual metric_value evaluate(double, double) const = 0;
};


}}} /* namespace monsoon::expressions::operators */

#endif /* MONSOON_EXPRESSIONS_OPERATORS_NUMERIC_BINOP_H */
