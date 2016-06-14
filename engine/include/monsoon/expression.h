#ifndef MONSOON_EXPRESSION_H
#define MONSOON_EXPRESSION_H

#include <monsoon/context.h>
#include <monsoon/metric_value.h>
#include <monsoon/tags.h>
#include <unordered_map>

namespace monsoon {


class expression {
 public:
  expression() noexcept = default;
  virtual ~expression() noexcept;

  virtual std::unordered_map<tags, metric_value> evaluate(const context&)
      const = 0;
  virtual std::string config_string() const = 0;

 protected:
  expression(const expression&) noexcept = default;
  expression(expression&&) noexcept {}
};


} /* namespace monsoon */

#endif /* MONSOON_EXPRESSION_H */
