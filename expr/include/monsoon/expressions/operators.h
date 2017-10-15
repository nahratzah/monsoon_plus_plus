#ifndef MONSOON_EXPRESSIONS_OPERATORS_H
#define MONSOON_EXPRESSIONS_OPERATORS_H

#include <monsoon/expression.h>
#include <monsoon/expr_export_.h>

namespace monsoon {
namespace expressions {


monsoon_expr_export_
expression_ptr logical_not(expression_ptr);
monsoon_expr_export_
expression_ptr logical_and(expression_ptr, expression_ptr);
monsoon_expr_export_
expression_ptr logical_or(expression_ptr, expression_ptr);
monsoon_expr_export_
expression_ptr logical_xor(expression_ptr, expression_ptr);

monsoon_expr_export_
expression_ptr cmp_eq(expression_ptr, expression_ptr);
monsoon_expr_export_
expression_ptr cmp_ne(expression_ptr, expression_ptr);
monsoon_expr_export_
expression_ptr cmp_lt(expression_ptr, expression_ptr);
monsoon_expr_export_
expression_ptr cmp_gt(expression_ptr, expression_ptr);
monsoon_expr_export_
expression_ptr cmp_le(expression_ptr, expression_ptr);
monsoon_expr_export_
expression_ptr cmp_ge(expression_ptr, expression_ptr);

monsoon_expr_export_
expression_ptr numeric_negate(expression_ptr);
monsoon_expr_export_
expression_ptr numeric_add(expression_ptr, expression_ptr);
monsoon_expr_export_
expression_ptr numeric_subtract(expression_ptr, expression_ptr);
monsoon_expr_export_
expression_ptr numeric_multiply(expression_ptr, expression_ptr);
monsoon_expr_export_
expression_ptr numeric_divide(expression_ptr, expression_ptr);
monsoon_expr_export_
expression_ptr numeric_modulo(expression_ptr, expression_ptr);


}} /* namespace monsoon::expressions */

#endif /* MONSOON_EXPRESSIONS_OPERATORS_H */
