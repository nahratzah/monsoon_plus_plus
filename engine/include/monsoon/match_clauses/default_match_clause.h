#ifndef MONSOON_MATCH_CLAUSES_DEFAULT_MATCH_CLAUSE_H
#define MONSOON_MATCH_CLAUSES_DEFAULT_MATCH_CLAUSE_H

#include <monsoon/match_clause.h>

namespace monsoon {
namespace match_clauses {


class default_match_clause
: public match_clause
{
 public:
  default_match_clause() noexcept;
  ~default_match_clause() noexcept;

  std::unordered_map<tags, metric_value> apply(
      std::unordered_map<tags, metric_value>,
      std::unordered_map<tags, metric_value>,
      std::function<metric_value(metric_value, metric_value)>) const override;
  void do_ostream(std::ostream&) const override;
};


}} /* namespace monsoon::match_clauses */

#include "default_match_clause-inl.h"

#endif /* MONSOON_MATCH_CLAUSES_DEFAULT_MATCH_CLAUSE_H */
