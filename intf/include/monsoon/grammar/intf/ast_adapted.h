#ifndef MONSOON_GRAMMAR_INTF_AST_ADAPTED_H
#define MONSOON_GRAMMAR_INTF_AST_ADAPTED_H

#include <monsoon/grammar/intf/ast.h>
#include <boost/fusion/adapted.hpp>

BOOST_FUSION_ADAPT_STRUCT(monsoon::grammar::ast::histogram_range_expr,
    lo,
    hi,
    count);
BOOST_FUSION_ADAPT_STRUCT(monsoon::grammar::ast::group_name_lit_expr,
    path,
    tags);

#endif /* MONSOON_GRAMMAR_INTF_AST_ADAPTED_H */
