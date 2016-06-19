#ifndef MONSOON_EXPRESSIONS_CONSTANT_H
#define MONSOON_EXPRESSIONS_CONSTANT_H

#include <monsoon/histogram.h>
#include <monsoon/expression.h>
#include <monsoon/metric_value.h>
#include <string>

namespace monsoon {
namespace expressions {
namespace constants {


class constant final
: public expression
{
 public:
  explicit constant(bool) noexcept;
  explicit constant(long) noexcept;
  explicit constant(unsigned long) noexcept;
  explicit constant(double) noexcept;
  explicit constant(std::string) noexcept;
  explicit constant(histogram) noexcept;
  constant(metric_value);
  ~constant() noexcept override;

  virtual expr_result evaluate(const context&) const override;
  virtual void do_ostream(std::ostream&) const override;

 private:
  metric_value value_;
};


}}} /* namespace monsoon::expressions::constants */

#include "constant-inl.h"

#endif /* MONSOON_EXPRESSIONS_CONSTANT_H */
