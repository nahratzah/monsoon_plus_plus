#ifndef MONSOON_MATCH_CLAUSES_BY_CLAUSE_H
#define MONSOON_MATCH_CLAUSES_BY_CLAUSE_H

#include <monsoon/optional.h>
#include <monsoon/match_clause.h>
#include <map>
#include <string>
#include <unordered_set>

namespace monsoon {
namespace match_clauses {


class by_clause
: public match_clause
{
 private:
  using mapping =
      std::map<tags, std::unordered_map<tags, metric_value>::iterator>;

 public:
  by_clause() = delete;
  template<typename Iter> by_clause(Iter, Iter, bool = false);
  ~by_clause() noexcept override;

  expr_result apply(
      expr_result,
      expr_result,
      std::function<metric_value(metric_value, metric_value)>) const override;
  void do_ostream(std::ostream&) const override;

 private:
  optional<tags> reduce_tag_(const tags&) const;
  mapping map_(std::unordered_map<tags, metric_value>&) const;
  tags select_tags_(const tags&, const tags&, const tags&) const;

  std::unordered_set<std::string> tag_names_;
  bool keep_common_;
};


}} /* namespace monsoon::match_clauses */

#include "by_clause-inl.h"

#endif /* MONSOON_MATCH_CLAUSES_BY_CLAUSE_H */
