#ifndef MONSOON_EXPRESSIONS_OPERATORS_UNOP_H
#define MONSOON_EXPRESSIONS_OPERATORS_UNOP_H

#include <monsoon/context.h>
#include <monsoon/expression.h>
#include <monsoon/metric_value.h>
#include <monsoon/tags.h>
#include <iosfwd>
#include <memory>
#include <unordered_map>

namespace monsoon {
namespace expressions {
namespace operators {


class unop
: public expression
{
 public:
  unop() = delete;
  unop(const char*, std::unique_ptr<expression>);
  ~unop() noexcept override;

  void do_ostream(std::ostream&) const override;
  std::unordered_map<tags, metric_value> evaluate(const context&) const final;

 protected:
  virtual metric_value evaluate(const metric_value&) const = 0;

 private:
  std::string symbol_;
  std::unique_ptr<expression> x_;
};


}}} /* namespace monsoon::expressions::operators */

#endif /* MONSOON_EXPRESSIONS_OPERATORS_UNOP_H */
