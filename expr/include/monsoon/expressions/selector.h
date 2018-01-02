#ifndef MONSOON_EXPRESSIONS_SELECTOR_H
#define MONSOON_EXPRESSIONS_SELECTOR_H

///\file
///\brief A metric-selecting expression.
///\ingroup expr

#include <monsoon/expr_export_.h>
#include <monsoon/expression.h>
#include <monsoon/path_matcher.h>
#include <monsoon/tag_matcher.h>
#include <optional>

namespace monsoon {
namespace expressions {


///@{
/**
 * \brief Create a selection expression.
 * \ingroup expr
 * \param groupname A matcher that matches on group names (exclusing tags).
 * \param metricname A matcher that matches on metric names.
 */
monsoon_expr_export_
auto selector(path_matcher groupname, path_matcher metricname)
    -> expression_ptr;
/**
 * \brief Create a selection expression, that filters on tags.
 * \ingroup expr
 * \param groupname A matcher that matches on group names (exclusing tags).
 * \param tagset A matcher that matches on the tags of a group name.
 * \param metricname A matcher that matches on metric names.
 */
monsoon_expr_export_
auto selector(path_matcher groupname, tag_matcher tagset, path_matcher metricname)
    -> expression_ptr;
/**
 * \brief Create a selection expression, that filters on tags.
 * \ingroup expr
 * \param groupname A matcher that matches on group names (exclusing tags).
 * \param tagset \parblock
 *   If present, the selector expression will check tags using the tag matcher.
 *   Otherwise, tags will not be checked.
 * \endparblock
 * \param metricname A matcher that matches on metric names.
 */
monsoon_expr_export_
auto selector(path_matcher groupname, std::optional<tag_matcher> tagset, path_matcher metricname)
    -> expression_ptr;
///@}


}} /* namespace monsoon::expressions */

#endif /* MONSOON_EXPRESSIONS_SELECTOR_H */
