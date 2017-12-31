#ifndef MONSOON_GRAMMAR_METRIC_VALUE_AST_H
#define MONSOON_GRAMMAR_METRIC_VALUE_AST_H

#include <monsoon/intf_export_.h>
#include <cmath>
#include <monsoon/metric_value.h>
#include <monsoon/simple_group.h>
#include <monsoon/metric_name.h>
#include <monsoon/group_name.h>
#include <monsoon/tags.h>
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

struct tags_lit_expr
: std::vector<std::tuple<std::string, value_expr>>
{
  operator tags() const { return tags(begin(), end()); }
};

struct simple_path_lit_expr
: std::vector<std::string>
{
  monsoon_intf_export_ operator metric_name() const;
  monsoon_intf_export_ operator simple_group() const;
};

struct group_name_lit_expr {
  simple_path_lit_expr path;
  tags_lit_expr tags;

  operator group_name() const { return group_name(path, tags); }
};


}}} /* namespace monsoon::grammar::ast */

#endif /* MONSOON_GRAMMAR_METRIC_VALUE_AST_H */
