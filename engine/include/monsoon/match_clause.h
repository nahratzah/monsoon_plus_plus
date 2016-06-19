#ifndef MONSOON_MATCH_CLAUSE_H
#define MONSOON_MATCH_CLAUSE_H

#include <monsoon/metric_value.h>
#include <monsoon/tags.h>
#include <monsoon/expr_result.h>
#include <functional>
#include <iosfwd>
#include <string>

namespace monsoon {


class match_clause {
 public:
  match_clause(bool = false) noexcept;
  virtual ~match_clause() noexcept;

  virtual expr_result apply(
      expr_result,
      expr_result,
      std::function<metric_value(metric_value, metric_value)>) const = 0;
  virtual void do_ostream(std::ostream&) const = 0;
  std::string config_string() const;

  const bool empty_config_string;
};


std::ostream& operator<<(std::ostream&, const match_clause&);


} /* namespace monsoon */

#include "match_clause.h"

#endif /* MONSOON_MATCH_CLAUSE_H */
