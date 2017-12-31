#ifndef MONSOON_GRAMMAR_INTF_AST_ADAPTED_H
#define MONSOON_GRAMMAR_INTF_AST_ADAPTED_H

#include <monsoon/grammar/intf/ast.h>
#include <boost/fusion/adapted.hpp>

BOOST_FUSION_ADAPT_STRUCT(monsoon::grammar::ast::histogram_range_expr,
    lo,
    hi,
    count);

#endif /* MONSOON_GRAMMAR_INTF_AST_ADAPTED_H */
