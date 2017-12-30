#ifndef MONSOON_GRAMMAR_EXPRESSION_AST_ADAPTED_H
#define MONSOON_GRAMMAR_EXPRESSION_AST_ADAPTED_H

#include <monsoon/grammar/expression/ast.h>
#include <boost/fusion/adapted.hpp>

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

#endif /* MONSOON_GRAMMAR_EXPRESSION_AST_ADAPTED_H */
