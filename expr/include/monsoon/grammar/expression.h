#ifndef MONSOON_GRAMMAR_EXPRESSION_H
#define MONSOON_GRAMMAR_EXPRESSION_H

#include <boost/spirit/include/qi.hpp>
#include <boost/phoenix/phoenix.hpp>
#include <monsoon/expression.h>
#include <monsoon/expressions/operators.h>
#include <monsoon/expressions/constant.h>

///\file
///\ingroup grammar

namespace monsoon {
namespace grammar {


namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;
namespace phoenix = boost::phoenix;

template<typename Iter, typename Skipper = ascii::space_type>
struct expression
: public qi::grammar<Iter, Skipper, expression_ptr()>
{
  using rule = qi::rule<
      Iter,
      Skipper,
      expression_ptr()>;

 public:
  expression()
  : expression::base_type(logical_or_)
  {
    using namespace monsoon::expressions;
    using phoenix::bind;
    using phoenix::placeholders::arg1;
    using phoenix::placeholders::arg2;
    using qi::_val;
    using qi::_1;
    using qi::_2;

    logical_or_ =
          (logical_or_ >> "||" >> logical_and_)[&logical_or]
        | logical_and_;
    logical_and_ =
          (logical_and_ >> "&&" >> equality_)[&logical_and]
        | equality_;
    equality_ =
          (equality_ >> '=' >> compare_)[&cmp_eq]
        | (equality_ >> "!=" >> compare_)[&cmp_ne]
        | compare_;
    compare_ =
          (compare_ >> ">=" >> shift_)[&cmp_ge]
        | (compare_ >> "<=" >> shift_)[&cmp_le]
        | (compare_ >> '>' >> shift_)[&cmp_gt]
        | (compare_ >> '<' >> shift_)[&cmp_lt]
        | shift_;
    shift_ =
          (shift_ >> "<<" >> add_subtract_)[&numeric_shift_left]
        | (shift_ >> ">>" >> add_subtract_)[&numeric_shift_right]
        | add_subtract_;
    add_subtract_ =
          (add_subtract_ >> '+' >> multiply_divide_)[&numeric_add]
        | (add_subtract_ >> '-' >> multiply_divide_)[&numeric_subtract]
        | multiply_divide_;
    multiply_divide_ =
          (multiply_divide_ >> '*' >> negate_)[&numeric_multiply]
        | (multiply_divide_ >> '/' >> negate_)[&numeric_divide]
        | (multiply_divide_ >> '%' >> negate_)[&numeric_modulo]
        | negate_;
    negate_ =
          ('!' >> negate_)[&logical_not]
        | ('-' >> negate_)[&numeric_negate]
        | braces_;
    braces_ =
          '(' >> logical_or_ >> ')'
#if 0 // XXX notyet
        | function_
        | selector_
#endif
        | value_;
    value_ =
          qi::lit("true")[([]() { return constant(metric_value(true)); })]
        | qi::lit("false")[([]() { return constant(metric_value(false)); })]
        | qi::uint_parser<metric_value::unsigned_type>()[([](metric_value::unsigned_type i) { return constant(metric_value(i)); })]
        | qi::int_parser<metric_value::signed_type>()[( [](metric_value::signed_type i) { return constant(metric_value(i)); })]
        | qi::double_[([](metric_value::fp_type i) { return constant(metric_value(i)); })];
  }

 private:
  rule logical_or_,
       logical_and_,
       equality_,
       compare_,
       shift_,
       add_subtract_,
       multiply_divide_,
       negate_,
       braces_,
#if 0 // XXX notyet
       function,
       selector,
#endif
       value_;
};


}} /* namespace monsoon::grammar */

#endif /* MONSOON_GRAMMAR_EXPRESSION_H */
