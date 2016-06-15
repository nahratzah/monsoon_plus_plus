#ifndef MONSOON_EXPRESSIONS_OPERATOR_BINOP_H
#define MONSOON_EXPRESSIONS_OPERATOR_BINOP_H

#include <monsoon/context.h>
#include <monsoon/expression.h>
#include <monsoon/metric_value.h>
#include <monsoon/tags.h>
#include <memory>
#include <unordered_map>

namespace monsoon {
namespace expressions {
namespace operators {


class binop
: public expression
{
 public:
  binop() = delete;
  binop(const char*, std::unique_ptr<expression>, std::unique_ptr<expression>);
  ~binop() noexcept override;

  std::string config_string() const override;
  std::unordered_map<tags, metric_value> evaluate(const context&) const final;

 protected:
  virtual metric_value evaluate(const metric_value&, const metric_value&)
      const = 0;

 private:
  std::string symbol_;
  std::unique_ptr<expression> x_, y_;
};


}}} /* namespace monsoon::expressions::operators */

#endif /* MONSOON_EXPRESSIONS_OPERATOR_BINOP_H */
