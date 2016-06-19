#ifndef MONSOON_EXPRESSIONS_OPERATOR_BINOP_H
#define MONSOON_EXPRESSIONS_OPERATOR_BINOP_H

#include <monsoon/context.h>
#include <monsoon/expression.h>
#include <monsoon/metric_value.h>
#include <monsoon/tags.h>
#include <monsoon/match_clause.h>
#include <iosfwd>
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
  binop(const char*, std::unique_ptr<expression>, std::unique_ptr<expression>,
        std::unique_ptr<match_clause>);
  ~binop() noexcept override;

  void do_ostream(std::ostream&) const override;
  expr_result evaluate(const context&) const final;

 protected:
  virtual metric_value evaluate(const metric_value&, const metric_value&)
      const = 0;

 private:
  std::string symbol_;
  std::unique_ptr<expression> x_, y_;
  std::unique_ptr<match_clause> matcher_;
};


}}} /* namespace monsoon::expressions::operators */

#endif /* MONSOON_EXPRESSIONS_OPERATOR_BINOP_H */
