#ifndef MONSOON_EXPRESSION_H
#define MONSOON_EXPRESSION_H

#include <monsoon/context.h>
#include <monsoon/expr_result.h>
#include <monsoon/metric_value.h>
#include <monsoon/tags.h>
#include <iosfwd>
#include <string>

namespace monsoon {


class expression {
 public:
  expression() noexcept = default;
  virtual ~expression() noexcept;

  virtual expr_result evaluate(const context&) const = 0;
  virtual void do_ostream(std::ostream&) const = 0;
  std::string config_string() const;

 protected:
  expression(const expression&) noexcept = default;
  expression(expression&&) noexcept {}
};

std::ostream& operator<<(std::ostream&, const expression&);


} /* namespace monsoon */

#endif /* MONSOON_EXPRESSION_H */
