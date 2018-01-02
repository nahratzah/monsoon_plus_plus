#ifndef MONSOON_GRAMMAR_EXPRESSION_AST_H
#define MONSOON_GRAMMAR_EXPRESSION_AST_H

#include <monsoon/expr_export_.h>
#include <monsoon/metric_value.h>
#include <monsoon/histogram.h>
#include <monsoon/expression.h>
#include <monsoon/match_clause.h>
#include <monsoon/grammar/intf/ast.h>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <memory>

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

struct selector_expr {
  path_matcher_expr groupname;
  std::optional<tag_matcher_expr> tagset;
  path_matcher_expr metricname;

  monsoon_expr_export_ operator expression_ptr() const;
};

struct primary_expr
: x3::variant<
      constant_expr,
      x3::forward_ast<logical_or_expr>,
      x3::forward_ast<selector_expr>
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

struct by_clause_expr {
  std::vector<std::string> names;
  std::optional<match_clause_keep> keep;

  monsoon_expr_export_
  auto build() const -> std::shared_ptr<const match_clause>;
};

struct without_clause_expr {
  std::vector<std::string> names;

  monsoon_expr_export_
  auto build() const -> std::shared_ptr<const match_clause>;
};

struct default_clause_expr {
  monsoon_expr_export_
  auto build() const -> std::shared_ptr<const match_clause>;
};

struct match_clause_expr
: x3::variant<default_clause_expr, by_clause_expr, without_clause_expr>
{
  using base_type::base_type;
  using base_type::operator=;

  monsoon_expr_export_
  auto build() const -> std::shared_ptr<const match_clause>;
};

template<typename NestedExpr, typename Enum>
struct binop_expr {
  NestedExpr head;
  std::vector<std::tuple<Enum, match_clause_expr, NestedExpr>> tail;

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

struct logical_and_enum {};
using logical_and_expr = binop_expr<equality_expr, logical_and_enum>;

struct logical_or_enum {};
struct logical_or_expr
: binop_expr<logical_and_expr, logical_or_enum>
{};


template<typename NestedExpr, typename Enum>
binop_expr<NestedExpr, Enum>::operator expression_ptr() const {
  expression_ptr result = head;
  for (const auto& i : tail)
    result = apply(std::get<0>(i), std::get<1>(i).build(),
        std::move(result), std::get<2>(i));
  return result;
}

monsoon_expr_export_
expression_ptr apply(equality_enum, std::shared_ptr<const match_clause>,
    expression_ptr, expression_ptr);
monsoon_expr_export_
expression_ptr apply(compare_enum, std::shared_ptr<const match_clause>,
    expression_ptr, expression_ptr);
monsoon_expr_export_
expression_ptr apply(shift_enum, std::shared_ptr<const match_clause>,
    expression_ptr, expression_ptr);
monsoon_expr_export_
expression_ptr apply(addsub_enum, std::shared_ptr<const match_clause>,
    expression_ptr, expression_ptr);
monsoon_expr_export_
expression_ptr apply(muldiv_enum, std::shared_ptr<const match_clause>,
    expression_ptr, expression_ptr);
monsoon_expr_export_
expression_ptr apply(logical_and_enum, std::shared_ptr<const match_clause>,
    expression_ptr, expression_ptr);
monsoon_expr_export_
expression_ptr apply(logical_or_enum, std::shared_ptr<const match_clause>,
    expression_ptr, expression_ptr);


}}} /* namespace monsoon::grammar::ast */

#endif /* MONSOON_GRAMMAR_EXPRESSION_AST_H */
