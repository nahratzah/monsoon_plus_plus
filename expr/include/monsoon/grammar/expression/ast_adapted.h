#ifndef MONSOON_GRAMMAR_EXPRESSION_AST_ADAPTED_H
#define MONSOON_GRAMMAR_EXPRESSION_AST_ADAPTED_H

#include <monsoon/grammar/expression/ast.h>
#include <boost/fusion/adapted.hpp>

BOOST_FUSION_ADAPT_STRUCT(monsoon::grammar::ast::constant_expr,
    v);
BOOST_FUSION_ADAPT_STRUCT(monsoon::grammar::ast::selector_expr,
    groupname,
    tagset,
    metricname);
BOOST_FUSION_ADAPT_STRUCT(monsoon::grammar::ast::logical_negate_expr,
    v);
BOOST_FUSION_ADAPT_STRUCT(monsoon::grammar::ast::numeric_negate_expr,
    v);
BOOST_FUSION_ADAPT_STRUCT(monsoon::grammar::ast::muldiv_expr,
    head,
    tail);
BOOST_FUSION_ADAPT_STRUCT(monsoon::grammar::ast::addsub_expr,
    head,
    tail);
BOOST_FUSION_ADAPT_STRUCT(monsoon::grammar::ast::shift_expr,
    head,
    tail);
BOOST_FUSION_ADAPT_STRUCT(monsoon::grammar::ast::compare_expr,
    head,
    tail);
BOOST_FUSION_ADAPT_STRUCT(monsoon::grammar::ast::equality_expr,
    head,
    tail);
BOOST_FUSION_ADAPT_STRUCT(monsoon::grammar::ast::logical_and_expr,
    head,
    tail);
BOOST_FUSION_ADAPT_STRUCT(monsoon::grammar::ast::logical_or_expr,
    head,
    tail);
BOOST_FUSION_ADAPT_STRUCT(monsoon::grammar::ast::by_clause_expr,
    names,
    keep);
BOOST_FUSION_ADAPT_STRUCT(monsoon::grammar::ast::without_clause_expr,
    names);

#endif /* MONSOON_GRAMMAR_EXPRESSION_AST_ADAPTED_H */
