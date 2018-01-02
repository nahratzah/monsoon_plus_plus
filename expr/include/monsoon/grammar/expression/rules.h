#ifndef MONSOON_GRAMMAR_EXPRESSION_RULES_H
#define MONSOON_GRAMMAR_EXPRESSION_RULES_H

#include <monsoon/metric_value.h>
#include <monsoon/grammar/expression/ast.h>
#include <monsoon/grammar/expression/ast_adapted.h>
#include <boost/spirit/home/x3.hpp>
#include <monsoon/grammar/intf/rules.h>
#include <type_traits>

namespace monsoon {
namespace grammar {
namespace parser {


namespace x3 = boost::spirit::x3;

inline const auto constant =
    x3::rule<class constant, ast::constant_expr>("constant");
inline const auto selector =
    x3::rule<class selector, ast::selector_expr>("selector");
inline const auto braces =
    x3::rule<class braces, ast::logical_or_expr>("braces");
inline const auto primary =
    x3::rule<class primary, ast::primary_expr>("braces");
inline const auto unary =
    x3::rule<class unary, ast::unary_expr>("unary expression");
inline const auto logical_negate =
    x3::rule<class logical_negate, ast::logical_negate_expr>("logical negation");
inline const auto numeric_negate =
    x3::rule<class numeric_negate, ast::numeric_negate_expr>("numeric negation");
inline const auto muldiv =
    x3::rule<class muldiv, ast::muldiv_expr>("multiplication or division");
inline const auto addsub =
    x3::rule<class addsub, ast::addsub_expr>("addition or subtraction");
inline const auto shift =
    x3::rule<class shift, ast::shift_expr>("shift operation");
inline const auto compare =
    x3::rule<class compare, ast::compare_expr>("comparison");
inline const auto equality =
    x3::rule<class equality, ast::equality_expr>("equality expression");
inline const auto logical_and =
    x3::rule<class logical_and, ast::logical_and_expr>("logical and expression");
inline const auto logical_or =
    x3::rule<class logical_or, ast::logical_or_expr>("logical or expression");
inline const auto by_clause =
    x3::rule<class by_clause, ast::by_clause_expr>("by clause");
inline const auto without_clause =
    x3::rule<class without_clause, ast::without_clause_expr>("without clause");
inline const auto match_clause =
    x3::rule<class match_clause_, ast::match_clause_expr>("match clause");


struct muldiv_sym
: x3::symbols<ast::muldiv_enum>
{
  monsoon_expr_export_ muldiv_sym();
};
inline const struct muldiv_sym muldiv_sym;

struct addsub_sym
: x3::symbols<ast::addsub_enum>
{
  monsoon_expr_export_ addsub_sym();
};
inline const struct addsub_sym addsub_sym;

struct shift_sym
: x3::symbols<ast::shift_enum>
{
  monsoon_expr_export_ shift_sym();
};
inline const struct shift_sym shift_sym;

struct compare_sym
: x3::symbols<ast::compare_enum>
{
  monsoon_expr_export_ compare_sym();
};
inline const struct compare_sym compare_sym;

struct equality_sym
: x3::symbols<ast::equality_enum>
{
  monsoon_expr_export_ equality_sym();
};
inline const struct equality_sym equality_sym;

struct match_clause_keep_sym
: x3::symbols<match_clause_keep>
{
  monsoon_expr_export_ match_clause_keep_sym();
};
inline const struct match_clause_keep_sym match_clause_keep_sym;


inline const auto constant_def = value;
inline const auto selector_def =
    path_matcher >>
    -tag_matcher >>
    path_matcher;
inline const auto braces_def =
    x3::lit('(') >> logical_or >> x3::lit(')');
inline const auto primary_def =
      constant
    | braces
    | selector;
inline const auto unary_def =
      primary
    | logical_negate
    | numeric_negate;
inline const auto logical_negate_def = x3::lit('!') >> unary;
inline const auto numeric_negate_def = x3::lit('-') >> unary;
inline const auto muldiv_def =
    unary >>
    *(muldiv_sym >> match_clause >> unary);
inline const auto addsub_def =
    muldiv >>
    *(addsub_sym >> match_clause >> muldiv);
inline const auto shift_def =
    addsub >>
    *(shift_sym >> match_clause >> addsub);
inline const auto compare_def =
    shift >>
    *(compare_sym >> match_clause >> shift);
inline const auto equality_def =
    compare >>
    *(equality_sym >> match_clause >> compare);
inline const auto logical_and_def =
    equality >>
    *(  x3::lit("&&") >> x3::attr(ast::logical_and_enum()) >>
        match_clause >> equality);
inline const auto logical_or_def =
    logical_and >>
    *(  x3::lit("||") >> x3::attr(ast::logical_or_enum()) >>
        match_clause >> logical_and);
inline const auto by_clause_def =
    x3::lit("by") >> x3::lit('(') >>
    (identifier | quoted_identifier) % ',' >>
    x3::lit(')') >>
    -(x3::lit("keep") >> match_clause_keep_sym);
inline const auto without_clause_def =
    x3::lit("by") >> x3::lit('(') >>
    (identifier | quoted_identifier) % ',' >>
    x3::lit(')');
inline const auto match_clause_def =
      by_clause
    | without_clause
    | x3::attr(ast::default_clause_expr());

BOOST_SPIRIT_DEFINE(
    constant,
    selector,
    braces,
    primary,
    unary,
    logical_negate,
    numeric_negate,
    muldiv,
    addsub,
    shift,
    compare,
    equality,
    logical_and,
    logical_or,
    by_clause,
    without_clause,
    match_clause);


} /* namespace monsoon::grammar::parser */


inline const auto& expression = parser::logical_or;
using expression_result = std::remove_reference_t<decltype(expression)>::attribute_type;


}} /* namespace monsoon::grammar */

#endif /* MONSOON_GRAMMAR_EXPRESSION_RULES_H */
