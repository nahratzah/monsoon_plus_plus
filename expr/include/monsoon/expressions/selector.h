#ifndef MONSOON_EXPRESSIONS_SELECTOR_H
#define MONSOON_EXPRESSIONS_SELECTOR_H

///\file
///\brief A metric-selecting expression.
///\ingroup expr

#include <monsoon/expr_export_.h>
#include <monsoon/expression.h>
#include <monsoon/path_matcher.h>
#include <optional>
#include <iosfwd>
#include <string>
#include <variant>
#include <unordered_map>

namespace monsoon {
namespace expressions {


/**
 * \brief A predicate on tags.
 * \ingroup expr
 *
 * Test if a \ref monsoon::tags "tag set" matches.
 */
class monsoon_expr_export_ tag_matcher {
  friend std::ostream& operator<<(std::ostream&, const tag_matcher&);

 public:
  /**
   * \brief Comparison enum.
   */
  enum comparison {
    eq, ///< Test for equality.
    ne, ///< Test for inequality.
    lt, ///< Test for less than.
    gt, ///< Test for greater than.
    le, ///< Test for less than or equal.
    ge  ///< Test for greater than or equal.
  };

  /**
   * \brief Comparison with a metric value constant.
   */
  using comparison_match = std::tuple<comparison, metric_value>;
  /**
   * \brief Require presence of a given tag.
   */
  struct presence_match {};
  /**
   * \brief Require absence of a given tag.
   */
  struct absence_match {};
  /**
   * \brief Match element.
   */
  using match_element =
      std::variant<absence_match, presence_match, comparison_match>;

 private:
  using matcher_map = std::unordered_multimap<std::string, match_element>;

 public:
  /**
   * \brief Iterator over match elements.
   */
  using const_iterator = matcher_map::const_iterator;
  ///@copydoc monsoon::expressions::tag_matcher::const_iterator
  using iterator = const_iterator;

  /**
   * \brief Test if the given tag set is a match.
   * \param tag_set The map of tags to test.
   * \return True if the tag_set matches this predicate, false otherwise.
   */
  bool operator()(const tags& tag_set) const;

  ///@{
  /**
   * \brief Iterate over match elements.
   */
  const_iterator begin() const;
  /**
   * \brief Iterate over match elements.
   */
  const_iterator end() const;
  /**
   * \brief Iterate over match elements.
   */
  const_iterator cbegin() const;
  /**
   * \brief Iterate over match elements.
   */
  const_iterator cend() const;
  ///@}

  ///@{
  /**
   * \brief Add a comparison check.
   * \note Comparison against a value implies a presence check.
   * \param name The name of the tag to compare with.
   * \param cmp The style of comparison.
   * \param value The value to compare against.
   */
  void check_comparison(std::string&& name, comparison cmp, metric_value&& value);
  /**
   * \brief Add a comparison check.
   * \note Comparison against a value implies a presence check.
   * \param name The name of the tag to compare with.
   * \param cmp The style of comparison.
   * \param value The value to compare against.
   */
  void check_comparison(std::string_view name, comparison cmp, const metric_value& value);
  /**
   * \brief Add a presence check.
   * \param name The name of the tag to check for presence.
   */
  void check_presence(std::string&& name);
  /**
   * \brief Add a presence check.
   * \param name The name of the tag to check for presence.
   */
  void check_presence(std::string_view name);
  /**
   * \brief Add an absence check.
   * \param name The name of the tag to check for absence.
   */
  void check_absence(std::string&& name);
  /**
   * \brief Add an absence check.
   * \param name The name of the tag to check for absence.
   */
  void check_absence(std::string_view name);
  ///@}

 private:
  matcher_map matcher_;
};


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

///\ingroup expr_io
///@{
/**
 * \brief Stream matcher textual representation to stream.
 *
 * \param out The output stream to write the textual representation.
 * \param m The matcher for which to create a textual representation.
 * \return \p out
 */
monsoon_expr_export_
std::ostream& operator<<(std::ostream& out, const tag_matcher& m);
/**
 * \brief Yield textual representation of the matcher.
 *
 * \param m The matcher for which to create a textual representation.
 * \return Textual representation of \p m
 */
monsoon_expr_export_
std::string to_string(const tag_matcher& m);
///@}


}} /* namespace monsoon::expressions */

#include "selector-inl.h"

#endif /* MONSOON_EXPRESSIONS_SELECTOR_H */
