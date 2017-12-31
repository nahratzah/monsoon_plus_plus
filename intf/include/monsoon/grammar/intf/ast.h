#ifndef MONSOON_GRAMMAR_METRIC_VALUE_AST_H
#define MONSOON_GRAMMAR_METRIC_VALUE_AST_H

#include <monsoon/intf_export_.h>
#include <monsoon/metric_value.h>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>

namespace monsoon {
namespace grammar {


namespace x3 = boost::spirit::x3;


namespace ast {


struct value_expr
: x3::variant<
      bool,
      metric_value::signed_type,
      metric_value::unsigned_type,
      metric_value::fp_type,
      std::string,
      histogram
    >
{
  using base_type::base_type;
  using base_type::operator=;

  monsoon_intf_export_ operator metric_value() const;
};


}}} /* namespace monsoon::grammar::ast */

#endif /* MONSOON_GRAMMAR_METRIC_VALUE_AST_H */
