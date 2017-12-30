#ifndef MONSOON_GRAMMAR_EXPRESSION_RULES_H
#define MONSOON_GRAMMAR_EXPRESSION_RULES_H

#include <monsoon/metric_value.h>
#include <monsoon/grammar/expression/ast.h>
#include <monsoon/grammar/expression/ast_adapted.h>
#include <boost/spirit/home/x3.hpp>
#include <type_traits>

namespace monsoon {
namespace grammar {
namespace parser {


namespace x3 = boost::spirit::x3;
namespace ascii = x3::ascii;

inline const auto constant =
    x3::rule<class constant, ast::constant_expr>("constant");
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


inline const auto constant_def =
      x3::bool_
    | x3::uint_parser<metric_value::unsigned_type>()
    | x3::int_parser<metric_value::signed_type>()
    | x3::double_;
inline const auto braces_def =
    x3::lit('(') >> logical_or >> x3::lit(')');
inline const auto primary_def =
      constant
    | braces;
inline const auto unary_def =
      primary
    | logical_negate
    | numeric_negate;
inline const auto logical_negate_def = x3::lit('!') >> unary;
inline const auto numeric_negate_def = x3::lit('-') >> unary;
inline const auto muldiv_def = unary >>
    *( ( x3::lit('*') >> x3::attr(ast::muldiv_enum::mul) >> unary
       | x3::lit('/') >> x3::attr(ast::muldiv_enum::div) >> unary
       | x3::lit('%') >> x3::attr(ast::muldiv_enum::mod) >> unary
       )
     );
inline const auto addsub_def = muldiv >>
    *( ( x3::lit('+') >> x3::attr(ast::addsub_enum::add) >> muldiv
       | x3::lit('-') >> x3::attr(ast::addsub_enum::sub) >> muldiv
       )
     );
inline const auto shift_def = addsub >>
    *( ( x3::lit("<<") >> x3::attr(ast::shift_enum::left) >> addsub
       | x3::lit(">>") >> x3::attr(ast::shift_enum::right) >> addsub
       )
     );
inline const auto compare_def = shift >>
    *( ( x3::lit("<=") >> x3::attr(ast::compare_enum::le) >> shift
       | x3::lit(">=") >> x3::attr(ast::compare_enum::ge) >> shift
       | x3::lit('<') >> x3::attr(ast::compare_enum::lt) >> shift
       | x3::lit('>') >> x3::attr(ast::compare_enum::gt) >> shift
       )
     );
inline const auto equality_def = compare >>
    *( ( x3::lit("==") >> x3::attr(ast::equality_enum::eq) >> compare
       | x3::lit("!=") >> x3::attr(ast::equality_enum::ne) >> compare
       )
     );
inline const auto logical_and_def = equality % "&&";
inline const auto logical_or_def = logical_and % "||";
BOOST_SPIRIT_DEFINE(
    constant,
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
    logical_or
    );


} /* namespace monsoon::grammar::parser */


inline const auto& expression = parser::logical_or;
using expression_result = std::remove_reference_t<decltype(expression)>::attribute_type;


}} /* namespace monsoon::grammar */

#endif /* MONSOON_GRAMMAR_EXPRESSION_RULES_H */
