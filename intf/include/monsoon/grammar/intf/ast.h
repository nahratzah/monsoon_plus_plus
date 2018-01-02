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
#include <monsoon/path_matcher.h>
#include <monsoon/tag_matcher.h>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
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

struct path_matcher_expr
: std::vector<path_matcher::match_element>
{
  monsoon_intf_export_ operator path_matcher() const;
};

struct tag_matcher_expr
: std::vector<
    x3::variant<
      std::tuple<std::string, tag_matcher::presence_match>,
      std::tuple<std::string, tag_matcher::absence_match>,
      std::tuple<std::string, tag_matcher::comparison, value_expr>
    >
  >
{
  monsoon_intf_export_ operator tag_matcher() const;
};


}}} /* namespace monsoon::grammar::ast */

#endif /* MONSOON_GRAMMAR_METRIC_VALUE_AST_H */
