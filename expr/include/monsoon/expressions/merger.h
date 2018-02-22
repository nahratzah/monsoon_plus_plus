#ifndef MONSOON_EXPRESSIONS_MERGER_H
#define MONSOON_EXPRESSIONS_MERGER_H

///\file
///\ingroup expr

#include <monsoon/expr_export_.h>
#include <monsoon/expression.h>
#include <monsoon/match_clause.h>
#include <monsoon/metric_value.h>
#include <monsoon/time_point.h>

namespace monsoon::expressions {


auto monsoon_expr_export_ make_merger(
    metric_value(*fn)(const metric_value&, const metric_value&),
    std::shared_ptr<const match_clause> mc,
    std::shared_ptr<const match_clause> out_mc,
    time_point::duration slack,
    expression::scalar_objpipe&& x,
    expression::scalar_objpipe&& y)
-> expression::scalar_objpipe;


} /* namespace monsoon::expressions */

#include "merger-inl.h"

#endif /* MONSOON_EXPRESSIONS_MERGER_H */
