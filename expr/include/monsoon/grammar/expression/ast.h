#ifndef MONSOON_GRAMMAR_EXPRESSION_AST_H
#define MONSOON_GRAMMAR_EXPRESSION_AST_H

#include <monsoon/expr_export_.h>
#include <monsoon/metric_value.h>
#include <monsoon/histogram.h>
#include <monsoon/expression.h>
#include <monsoon/grammar/intf/ast.h>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <string>
#include <vector>
#include <utility>

namespace monsoon {
namespace grammar {


namespace x3 = boost::spirit::x3;


namespace ast {


struct logical_negate_expr;
struct numeric_negate_expr;
struct unary_expr;
struct logical_or_expr;


struct constant_expr {
  monsoon_expr_export_ operator expression_ptr() const;

  value_expr v;
};

struct primary_expr
: x3::variant<
      constant_expr,
      x3::forward_ast<logical_or_expr>
    >
{
  using base_type::base_type;
  using base_type::operator=;

  monsoon_expr_export_ operator expression_ptr() const;
};

struct unary_expr
: x3::variant<
      primary_expr,
      x3::forward_ast<logical_negate_expr>,
      x3::forward_ast<numeric_negate_expr>
    >
{
  using base_type::base_type;
  using base_type::operator=;

  monsoon_expr_export_ operator expression_ptr() const;
};

struct logical_negate_expr {
  logical_negate_expr() = default;
  logical_negate_expr(unary_expr v) : v(v) {}
  monsoon_expr_export_ operator expression_ptr() const;

  unary_expr v;
};

struct numeric_negate_expr {
  numeric_negate_expr() = default;
  numeric_negate_expr(unary_expr v) : v(v) {}
  monsoon_expr_export_ operator expression_ptr() const;

  unary_expr v;
};

template<typename NestedExpr, typename Enum>
struct binop_expr {
  NestedExpr head;
  std::vector<std::pair<Enum, NestedExpr>> tail;

  operator expression_ptr() const;
};

enum class muldiv_enum { mul, div, mod };
using muldiv_expr = binop_expr<unary_expr, muldiv_enum>;

enum class addsub_enum { add, sub };
using addsub_expr = binop_expr<muldiv_expr, addsub_enum>;

enum class shift_enum { left, right };
using shift_expr = binop_expr<addsub_expr, shift_enum>;

enum class compare_enum { ge, le, gt, lt };
using compare_expr = binop_expr<shift_expr, compare_enum>;

enum class equality_enum { eq, ne };
using equality_expr = binop_expr<compare_expr, equality_enum>;

struct logical_and_expr
: public std::vector<equality_expr>
{
  operator expression_ptr() const;
};

struct logical_or_expr
: public std::vector<logical_and_expr>
{
  operator expression_ptr() const;
};


template<typename NestedExpr, typename Enum>
binop_expr<NestedExpr, Enum>::operator expression_ptr() const {
  expression_ptr result = head;
  for (const auto& i : tail)
    result = apply(std::get<0>(i), std::move(result), std::get<1>(i));
  return result;
}

monsoon_expr_export_
expression_ptr apply(equality_enum, expression_ptr, expression_ptr);
monsoon_expr_export_
expression_ptr apply(compare_enum, expression_ptr, expression_ptr);
monsoon_expr_export_
expression_ptr apply(shift_enum, expression_ptr, expression_ptr);
monsoon_expr_export_
expression_ptr apply(addsub_enum, expression_ptr, expression_ptr);
monsoon_expr_export_
expression_ptr apply(muldiv_enum, expression_ptr, expression_ptr);


}}} /* namespace monsoon::grammar::ast */

#endif /* MONSOON_GRAMMAR_EXPRESSION_AST_H */
