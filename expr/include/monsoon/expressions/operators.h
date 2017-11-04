#ifndef MONSOON_EXPRESSIONS_OPERATORS_H
#define MONSOON_EXPRESSIONS_OPERATORS_H

///\file
///\ingroup expr

#include <monsoon/expression.h>
#include <monsoon/expr_export_.h>

namespace monsoon {
namespace expressions {


///\defgroup expr_operators Expression evaluation (operators)
///\ingroup expr
///@{

///@{
///\brief logical \em not operation
///
/// \code{.c} !x \endcode
monsoon_expr_export_
expression_ptr logical_not(expression_ptr x);
///\brief logican \em and operation
///
/// \code{.c} x && y \endcode
monsoon_expr_export_
expression_ptr logical_and(expression_ptr x, expression_ptr y);
///\brief logican \em or operation
///
/// \code{.c} x || y \endcode
monsoon_expr_export_
expression_ptr logical_or(expression_ptr x, expression_ptr y);
///@}

///@{
///\brief comparison expression
///
/// \code{.c} x == y \endcode
monsoon_expr_export_
expression_ptr cmp_eq(expression_ptr, expression_ptr);
///\brief comparison expression
///
/// \code{.c} x != y \endcode
monsoon_expr_export_
expression_ptr cmp_ne(expression_ptr, expression_ptr);
///\brief comparison expression
///
/// \code{.c} x < y \endcode
monsoon_expr_export_
expression_ptr cmp_lt(expression_ptr, expression_ptr);
///\brief comparison expression
///
/// \code{.c} x > y \endcode
monsoon_expr_export_
expression_ptr cmp_gt(expression_ptr, expression_ptr);
///\brief comparison expression
///
/// \code{.c} x <= y \endcode
monsoon_expr_export_
expression_ptr cmp_le(expression_ptr, expression_ptr);
///\brief comparison expression
///
/// \code{.c} x >= y \endcode
monsoon_expr_export_
expression_ptr cmp_ge(expression_ptr, expression_ptr);
///@}

///@{
///\brief algebra operation
///
/// \code{.c} -x \endcode
monsoon_expr_export_
expression_ptr numeric_negate(expression_ptr);
///\brief algebra operation
///
/// \code{.c} x + y \endcode
monsoon_expr_export_
expression_ptr numeric_add(expression_ptr, expression_ptr);
///\brief algebra operation
///
/// \code{.c} x - y \endcode
monsoon_expr_export_
expression_ptr numeric_subtract(expression_ptr, expression_ptr);
///\brief algebra operation
///
/// \code{.c} x * y \endcode
monsoon_expr_export_
expression_ptr numeric_multiply(expression_ptr, expression_ptr);
///\brief algebra operation
///
/// \code{.c} x / y \endcode
monsoon_expr_export_
expression_ptr numeric_divide(expression_ptr, expression_ptr);
///\brief algebra operation
///
/// \code{.c} x % y \endcode
monsoon_expr_export_
expression_ptr numeric_modulo(expression_ptr, expression_ptr);
///@}

///@}


}} /* namespace monsoon::expressions */

#endif /* MONSOON_EXPRESSIONS_OPERATORS_H */
