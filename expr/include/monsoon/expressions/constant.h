#ifndef MONSOON_EXPRESSIONS_CONSTANT_H
#define MONSOON_EXPRESSIONS_CONSTANT_H

///\file
///\brief Constant expression.
///\ingroup expr

#include <monsoon/expr_export_.h>
#include <monsoon/expression.h>
#include <monsoon/metric_value.h>

namespace monsoon {
namespace expressions {


/**
 * \brief Create a constant expression.
 * \ingroup expr
 *
 * The result expression will always yield the given \p value.
 * \param value The value of the constant expression.
 * \return An expression that emits the given value.
 */
monsoon_expr_export_
auto constant(metric_value value) -> expression_ptr;


}} /* namespace monsoon::expressions */

#endif /* MONSOON_EXPRESSIONS_CONSTANT_H */
