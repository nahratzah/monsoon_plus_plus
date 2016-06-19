#ifndef MONSOON_MATCH_CLAUSES_WITHOUT_CLAUSE_H
#define MONSOON_MATCH_CLAUSES_WITHOUT_CLAUSE_H

#include <monsoon/match_clause.h>
#include <map>
#include <string>
#include <unordered_set>

namespace monsoon {
namespace match_clauses {


class without_clause
: public match_clause
{
 private:
  using mapping =
      std::map<tags, std::unordered_map<tags, metric_value>::iterator>;

 public:
  without_clause() = delete;
  template<typename Iter> without_clause(Iter, Iter, bool = false);
  ~without_clause() noexcept override;

  std::unordered_map<tags, metric_value> apply(
      std::unordered_map<tags, metric_value>,
      std::unordered_map<tags, metric_value>,
      std::function<metric_value(metric_value, metric_value)>) const override;
  void do_ostream(std::ostream&) const override;

 private:
  tags reduce_tag_(const tags&) const;
  mapping map_(std::unordered_map<tags, metric_value>&) const;
  tags select_tags_(const tags&, const tags&, const tags&) const;

  std::unordered_set<std::string> tag_names_;
  bool keep_common_;
};


}} /* namespace monsoon::match_clauses */

#include "without_clause-inl.h"

#endif /* MONSOON_MATCH_CLAUSES_WITHOUT_CLAUSE_H */
