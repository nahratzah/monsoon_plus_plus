#ifndef MONSOON_PATH_MATCHER_H
#define MONSOON_PATH_MATCHER_H

///\file
///\brief A path matcher.
///\ingroup intf

#include <monsoon/intf_export_.h>
#include <monsoon/simple_group.h>
#include <monsoon/metric_name.h>
#include <iosfwd>
#include <string>
#include <vector>
#include <variant>

namespace monsoon {


/**
 * \brief A predicate on paths.
 * \ingroup intf
 *
 * Test if a \ref monsoon::simple_group "simple_group" or
 * \ref monsoon::metric_name "metric_name" matches.
 */
class monsoon_intf_export_ path_matcher {
 public:
  /**
   * \brief Segment literal.
   * Matches a path segment consisting of the given literal.
   * \note Case sensitive.
   */
  using literal = std::string;
  /**
   * \brief Single wildcard.
   * Matches any single path segment.
   */
  struct wildcard {};
  /**
   * \brief Double wildcard.
   * Matches zero or more path segments.
   */
  struct double_wildcard {};
  /**
   * \brief Match element, consisting of either a literal, a wildcard or a double_wildcard.
   */
  using match_element = std::variant<literal, wildcard, double_wildcard>;

 private:
  using matcher_vector = std::vector<match_element>;

 public:
  /**
   * \brief Iterator over match elements.
   */
  using const_iterator = matcher_vector::const_iterator;
  ///\copydoc monsoon::expressions::path_matcher::const_iterator
  using iterator = const_iterator;

  ///@{
  /**
   * \brief Test if the \ref monsoon::simple_group "simple group" matches.
   * \param path The path to test a match on.
   */
  bool operator()(const simple_group& path) const;
  /**
   * \brief Test if the \ref monsoon::metric_name "metric name" matches.
   * \param path The path to test a match on.
   */
  bool operator()(const metric_name& path) const;
  ///@}

  ///@{
  /**
   * \brief Iterate over match_segments.
   */
  const_iterator begin() const;
  /**
   * \brief Iterate over match_segments.
   */
  const_iterator end() const;
  /**
   * \brief Iterate over match_segments.
   */
  const_iterator cbegin() const;
  /**
   * \brief Iterate over match_segments.
   */
  const_iterator cend() const;
  ///@}

  ///@{
  /**
   * \brief Append a literal segment match to the path matcher.
   * \param lit The literal to match against.
   * \returns *this
   */
  path_matcher& push_back_literal(literal&& lit);
  /**
   * \brief Append a literal segment match to the path matcher.
   * \param lit The literal to match against.
   * \returns *this
   */
  path_matcher& push_back_literal(std::string_view lit);
  /**
   * \brief Append a literal segment match to the path matcher.
   * \param lit The literal to match against.
   * \returns *this
   */
  path_matcher& push_back_literal(const char* lit);
  /**
   * \brief Append a wildcard segment match to the path matcher.
   * \returns *this
   */
  path_matcher& push_back_wildcard();
  /**
   * \brief Append a double wildcard segment match to the path matcher.
   * \returns *this
   */
  path_matcher& push_back_double_wildcard();
  ///@}

 private:
  matcher_vector matcher_;
};

/**
 * \brief Test if two path matchers have overlap.
 * \ingroup intf
 * \relates path_matcher
 * \details
 * Two path matchers have overlap, if there exist any path, such that both matchers match.
 */
monsoon_intf_export_
auto has_overlap(const path_matcher& x, const path_matcher& y) -> bool;

///\ingroup intf_io
///@{
/**
 * \brief Stream matcher textual representation to stream.
 *
 * \param out The output stream to write the textual representation.
 * \param m The matcher for which to create a textual representation.
 * \return \p out
 */
monsoon_intf_export_
std::ostream& operator<<(std::ostream& out, const path_matcher& m);
/**
 * \brief Yield textual representation of the matcher.
 *
 * \param m The matcher for which to create a textual representation.
 * \return Textual representation of \p m
 */
monsoon_intf_export_
std::string to_string(const path_matcher& m);
///@}


} /* namespace monsoon */

#include "path_matcher-inl.h"

#endif /* MONSOON_PATH_MATCHER_H */
