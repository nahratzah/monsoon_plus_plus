#ifndef MONSOON_GRAMMAR_METRIC_VALUE_AST_H
#define MONSOON_GRAMMAR_METRIC_VALUE_AST_H

#include <monsoon/intf_export_.h>
#include <cmath>
#include <monsoon/metric_value.h>
#include <monsoon/simple_group.h>
#include <monsoon/metric_name.h>
#include <monsoon/histogram.h>
#include <boost/spirit/home/x3.hpp>
#include <string>
#include <vector>
#include <variant>

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
: std::variant<
      bool,
      metric_value::signed_type,
      metric_value::unsigned_type,
      metric_value::fp_type,
      std::string,
      histogram_expr
    >
{
  using base_type = std::variant<
      bool,
      metric_value::signed_type,
      metric_value::unsigned_type,
      metric_value::fp_type,
      std::string,
      histogram_expr
    >;

  using base_type::base_type;
  using base_type::operator=;

  monsoon_intf_export_ operator metric_value() const;
};

struct simple_path_lit_expr
: std::vector<std::string>
{
  monsoon_intf_export_ operator metric_name() const;
  monsoon_intf_export_ operator simple_group() const;
};


}}} /* namespace monsoon::grammar::ast */

#endif /* MONSOON_GRAMMAR_METRIC_VALUE_AST_H */
