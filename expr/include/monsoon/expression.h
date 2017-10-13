#ifndef MONSOON_EXPRESSION_H
#define MONSOON_EXPRESSION_H

#include <monsoon/expr_export_.h>
#include <monsoon/metric_value.h>
#include <monsoon/tags.h>
#include <monsoon/metric_source.h>
#include <monsoon/time_point.h>
#include <monsoon/time_range.h>
#include <variant>
#include <memory>
#include <iosfwd>

namespace monsoon {


class expression;

using expression_ptr = std::shared_ptr<const expression>;

class monsoon_expr_export_ expression {
  friend std::ostream& operator<<(std::ostream&, const expression&);

 public:
  using emit_type = std::variant<metric_value, std::tuple<tags, metric_value>>;

  template<typename Expr, typename... Args>
      static expression_ptr make_ptr(Args&&... args);

  virtual ~expression() noexcept;

  virtual void operator()(acceptor<emit_type>&, const metric_source&,
      const time_range&, time_point::duration) const = 0;

 private:
  virtual void do_ostream(std::ostream&) const = 0;
};

std::string to_string(const expression&);


} /* namespace monsoon */

#include "expression-inl.h"

#endif /* MONSOON_EXPRESSION_H */
