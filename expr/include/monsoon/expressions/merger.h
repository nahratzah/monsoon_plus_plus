#ifndef MONSOON_EXPRESSIONS_MERGER_H
#define MONSOON_EXPRESSIONS_MERGER_H

///\file
///\ingroup expr

#include <memory>
#include <variant>
#include <vector>
#include <monsoon/expr_export_.h>
#include <monsoon/expression.h>
#include <monsoon/match_clause.h>
#include <monsoon/metric_value.h>
#include <monsoon/time_point.h>

namespace monsoon::expressions {


/**
 * \brief Merge two scalar expressions using a computation.
 * \ingroup expr
 * \param[in] fn Compute the result of two metric values.
 * \param[in] mc Unused.
 * \param[in] out_mc Unused.
 * \param[in] slack Duration before and after generated time points, to consider if interpolation is required.
 * \param[in] x Objpipe supplying left argument to the \p fn.
 * \param[in] y Objpipe supplying left argument to the \p fn.
 */
auto monsoon_expr_export_ make_merger(
    metric_value(*fn)(const metric_value&, const metric_value&),
    std::shared_ptr<const match_clause> mc,
    std::shared_ptr<const match_clause> out_mc,
    time_point::duration slack,
    expression::scalar_objpipe&& x,
    expression::scalar_objpipe&& y)
-> expression::scalar_objpipe;

/**
 * \brief Merge two expressions using a computation.
 * \ingroup expr
 * \param[in] fn Compute the result of two metric values.
 * \param[in] mc Match clause used to join values.
 * \param[in] out_mc Match clause used on values in result objpipe.
 * \param[in] slack Duration before and after generated time points, to consider if interpolation is required.
 * \param[in] x Objpipe supplying left argument to the \p fn.
 * \param[in] y Objpipe supplying left argument to the \p fn.
 */
auto monsoon_expr_export_ make_merger(
    metric_value(*fn)(const metric_value&, const metric_value&),
    std::shared_ptr<const match_clause> mc,
    std::shared_ptr<const match_clause> out_mc,
    time_point::duration slack,
    expression::vector_objpipe&& x,
    expression::scalar_objpipe&& y)
-> expression::vector_objpipe;

/**
 * \brief Merge two expressions using a computation.
 * \ingroup expr
 * \param[in] fn Compute the result of two metric values.
 * \param[in] mc Match clause used to join values.
 * \param[in] out_mc Match clause used on values in result objpipe.
 * \param[in] slack Duration before and after generated time points, to consider if interpolation is required.
 * \param[in] x Objpipe supplying left argument to the \p fn.
 * \param[in] y Objpipe supplying left argument to the \p fn.
 */
auto monsoon_expr_export_ make_merger(
    metric_value(*fn)(const metric_value&, const metric_value&),
    std::shared_ptr<const match_clause> mc,
    std::shared_ptr<const match_clause> out_mc,
    time_point::duration slack,
    expression::scalar_objpipe&& x,
    expression::vector_objpipe&& y)
-> expression::vector_objpipe;

/**
 * \brief Merge two expressions using a computation.
 * \ingroup expr
 * \param[in] fn Compute the result of two metric values.
 * \param[in] mc Match clause used to join values.
 * \param[in] out_mc Match clause used on values in result objpipe.
 * \param[in] slack Duration before and after generated time points, to consider if interpolation is required.
 * \param[in] x Objpipe supplying left argument to the \p fn.
 * \param[in] y Objpipe supplying left argument to the \p fn.
 */
auto monsoon_expr_export_ make_merger(
    metric_value(*fn)(const metric_value&, const metric_value&),
    std::shared_ptr<const match_clause> mc,
    std::shared_ptr<const match_clause> out_mc,
    time_point::duration slack,
    expression::vector_objpipe&& x,
    expression::vector_objpipe&& y)
-> expression::vector_objpipe;

/**
 * \brief Merge expressions using a computation.
 * \ingroup expr
 * \param[in] fn Compute the result of two metric values.
 * \param[in] mc Match clause used to join values.
 * \param[in] out_mc Match clause used on values in result objpipe.
 * \param[in] slack Duration before and after generated time points, to consider if interpolation is required.
 * \param[in] pipes Objpipes supplying arguments to \p fn.
 */
auto monsoon_expr_export_ make_merger(
    std::function<metric_value(const std::vector<metric_value>&)> fn,
    std::shared_ptr<const match_clause> mc,
    std::shared_ptr<const match_clause> out_mc,
    time_point::duration slack,
    std::vector<std::variant<expression::scalar_objpipe, expression::vector_objpipe>>&& pipes)
-> std::variant<expression::scalar_objpipe, expression::vector_objpipe>;


} /* namespace monsoon::expressions */

#endif /* MONSOON_EXPRESSIONS_MERGER_H */
