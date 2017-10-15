#ifndef MONSOON_EXPRESSIONS_CONSTANT_H
#define MONSOON_EXPRESSIONS_CONSTANT_H

#include <monsoon/expr_export_.h>
#include <monsoon/expression.h>
#include <monsoon/metric_value.h>

namespace monsoon {
namespace expressions {


monsoon_expr_export_
auto constant(metric_value) -> expression_ptr;


}} /* namespace monsoon::expressions */

#endif /* MONSOON_EXPRESSIONS_CONSTANT_H */
