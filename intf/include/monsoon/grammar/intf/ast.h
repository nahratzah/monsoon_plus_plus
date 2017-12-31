#ifndef MONSOON_GRAMMAR_METRIC_VALUE_AST_H
#define MONSOON_GRAMMAR_METRIC_VALUE_AST_H

#include <monsoon/intf_export_.h>
#include <cmath>
#include <monsoon/metric_value.h>
#include <monsoon/histogram.h>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <string>
#include <vector>

namespace monsoon {
namespace grammar {


namespace x3 = boost::spirit::x3;


namespace ast {


struct histogram_range_expr {
  std::double_t lo, hi, count;

  operator std::pair<histogram::range, std::double_t>() const;
};

struct histogram_expr
: std::vector<histogram_range_expr>
{
  operator histogram() const;
};

struct value_expr
: x3::variant<
      bool,
      metric_value::signed_type,
      metric_value::unsigned_type,
      metric_value::fp_type,
      std::string,
      histogram_expr
    >
{
  using base_type::base_type;
  using base_type::operator=;

  monsoon_intf_export_ operator metric_value() const;
};


}}} /* namespace monsoon::grammar::ast */

#endif /* MONSOON_GRAMMAR_METRIC_VALUE_AST_H */
