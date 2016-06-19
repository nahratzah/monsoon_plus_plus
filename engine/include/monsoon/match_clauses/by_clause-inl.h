#ifndef MONSOON_MATCH_CLAUSES_BY_CLAUSE_INL_H
#define MONSOON_MATCH_CLAUSES_BY_CLAUSE_INL_H

namespace monsoon {
namespace match_clauses {


template<typename Iter>
by_clause::by_clause(Iter b, Iter e, bool keep_common)
: tag_names_(b, e),
  keep_common_(keep_common)
{}


}} /* namespace monsoon::match_clauses */

#endif /* MONSOON_MATCH_CLAUSES_BY_CLAUSE_INL_H */
