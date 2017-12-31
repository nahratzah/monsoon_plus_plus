#ifndef MONSOON_GRAMMAR_INTF_RULES_H
#define MONSOON_GRAMMAR_INTF_RULES_H

#include <monsoon/intf_export_.h>
#include <monsoon/grammar/intf/ast.h>
#include <monsoon/grammar/intf/ast_adapted.h>
#include <boost/spirit/home/x3.hpp>

namespace monsoon {
namespace grammar {
namespace parser {


namespace x3 = boost::spirit::x3;

inline const auto histogram_range =
    x3::rule<class histogram_range, ast::histogram_range_expr>("histogram range");
inline const auto histogram =
    x3::rule<class histogram_, ast::histogram_expr>("histogram");
inline const auto value =
    x3::rule<class value, ast::value_expr>("value");

inline const auto histogram_range_def =
    x3::double_ >> x3::lit("..") >> x3::double_ >> x3::lit('=') >> x3::double_;
inline const auto histogram_def =
    x3::lit('[') >> -(histogram_range % ',') >> x3::lit(']');
inline const auto value_def =
      x3::bool_
    | x3::uint_parser<metric_value::unsigned_type>()
    | x3::int_parser<metric_value::signed_type>()
    | x3::double_
    | histogram;

BOOST_SPIRIT_DEFINE(histogram_range);
BOOST_SPIRIT_DEFINE(histogram);
BOOST_SPIRIT_DEFINE(value);


}}} /* namespace monsoon::grammar::parser */

#endif /* MONSOON_GRAMMAR_INTF_RULES_H */
