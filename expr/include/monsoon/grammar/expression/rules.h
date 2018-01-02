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
inline const auto path_matcher =
    x3::rule<class path_matcher_, ast::path_matcher_expr>("path selector");
inline const auto tag_matcher =
    x3::rule<class tag_matcher_, ast::tag_matcher_expr>("tag selector");
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


struct muldiv_sym
: x3::symbols<ast::muldiv_enum>
{
  muldiv_sym() {
    add("*", ast::muldiv_enum::mul);
    add("/", ast::muldiv_enum::div);
    add("%", ast::muldiv_enum::mod);
  }
};
inline const struct muldiv_sym muldiv_sym;

struct addsub_sym
: x3::symbols<ast::addsub_enum>
{
  addsub_sym() {
    add("+", ast::addsub_enum::add);
    add("-", ast::addsub_enum::sub);
  }
};
inline const struct addsub_sym addsub_sym;

struct shift_sym
: x3::symbols<ast::shift_enum>
{
  shift_sym() {
    add("<<", ast::shift_enum::left);
    add(">>", ast::shift_enum::right);
  }
};
inline const struct shift_sym shift_sym;

struct compare_sym
: x3::symbols<ast::compare_enum>
{
  compare_sym() {
    add("<=", ast::compare_enum::le);
    add(">=", ast::compare_enum::ge);
    add("<", ast::compare_enum::lt);
    add(">", ast::compare_enum::gt);
  }
};
inline const struct compare_sym compare_sym;

struct equality_sym
: x3::symbols<ast::equality_enum>
{
  equality_sym() {
    add("=", ast::equality_enum::eq);
    add("!=", ast::equality_enum::ne);
  }
};
inline const struct equality_sym equality_sym;

struct tag_matcher_comparison_sym
: x3::symbols<tag_matcher::comparison>
{
  tag_matcher_comparison_sym() {
    add("=", tag_matcher::eq);
    add("!=", tag_matcher::ne);
    add("<", tag_matcher::lt);
    add(">", tag_matcher::gt);
    add("<=", tag_matcher::le);
    add(">=", tag_matcher::ge);
  }
};
inline const struct tag_matcher_comparison_sym tag_matcher_comparison_sym;


inline const auto constant_def = value;
inline const auto path_matcher_def =
    ( x3::lit("**") >> x3::attr(path_matcher::double_wildcard())
    | x3::lit('*') >> x3::attr(path_matcher::wildcard())
    | quoted_identifier
    | identifier
    ) % '.';
inline const auto tag_matcher_def =
    x3::lit('{') >>
    -(( (identifier | quoted_identifier) >> tag_matcher_comparison_sym >>
        tag_value
      | x3::lit('!') >> (identifier | quoted_identifier) >>
        x3::attr(tag_matcher::absence_match())
      | (identifier | quoted_identifier) >>
        x3::attr(tag_matcher::presence_match())
      ) % ',') >>
    x3::lit('}');
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
inline const auto muldiv_def = unary >> *(muldiv_sym >> unary);
inline const auto addsub_def = muldiv >> *(addsub_sym >> muldiv);
inline const auto shift_def = addsub >> *(shift_sym >> addsub);
inline const auto compare_def = shift >> *(compare_sym >> shift);
inline const auto equality_def = compare >> *(equality_sym >> compare);
inline const auto logical_and_def = equality % "&&";
inline const auto logical_or_def = logical_and % "||";
BOOST_SPIRIT_DEFINE(
    constant,
    path_matcher,
    tag_matcher,
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
    logical_or);


} /* namespace monsoon::grammar::parser */


inline const auto& expression = parser::logical_or;
using expression_result = std::remove_reference_t<decltype(expression)>::attribute_type;


}} /* namespace monsoon::grammar */

#endif /* MONSOON_GRAMMAR_EXPRESSION_RULES_H */
