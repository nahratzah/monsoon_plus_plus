#ifndef MONSOON_TAGS_H
#define MONSOON_TAGS_H

///\file
///\ingroup intf

#include <monsoon/intf_export_.h>
#include <monsoon/metric_value.h>
#include <optional>
#include <cstddef>
#include <functional>
#include <iosfwd>
#include <string>
#include <map>

namespace monsoon {


/**
 * \brief Representation of a tag set.
 * \ingroup intf
 *
 * A tag set is a map of names to tag values.
 *
 * Tag values are \ref metric_value "metric values",
 * excluding \ref metric_value::empty and \ref histogram values.
 */
class monsoon_intf_export_ tags {
 public:
  ///\brief Underlying map type.
  using map_type = std::map<std::string, metric_value>;
  ///\brief Iterator type.
  using iterator = map_type::const_iterator;

  ///@{
  tags() = default;

  /**
   * \brief Construct a tag set using an iteration.
   *
   * \tparam Iter Iterator type.
   * \param[in] b,e The iteration range to use to create the tag set.
   */
  template<typename Iter> tags(Iter b, Iter e);
  /**
   * \brief Construct a tag set using the given values.
   */
  explicit tags(map_type) noexcept;
  /**
   * \brief Construct a tag set using the given values.
   */
  explicit tags(std::initializer_list<map_type::value_type>);
  ///@}

  /**
   * \brief Parse tags expression.
   * \param s A string representation of a tag set.
   * \return A tag set, as represented by the expression.
   * \throw std::invalid_argument indicating \p s does not hold a valid expression.
   */
  static tags parse(std::string_view s);

  /**
   * \return true if the tag set is empty.
   */
  bool empty() const noexcept;
  /**
   * \return the underlying tag map.
   */
  const map_type& get_map() const noexcept;
  /**
   * \brief Lookup tag value.
   * \param name The tag name to look for.
   * \return
   *   the metric value for the given tag name,
   *   or an empty optional if it does not exist.
   */
  auto operator[](const std::string& name) const noexcept
      -> std::optional<metric_value>;

  ///@{
  ///\brief Tags are comparable.
  bool operator==(const tags&) const noexcept;
  ///\brief Tags are comparable.
  bool operator!=(const tags&) const noexcept;
  ///\brief Tags are comparable.
  bool operator<(const tags&) const noexcept;
  ///\brief Tags are comparable.
  bool operator>(const tags&) const noexcept;
  ///\brief Tags are comparable.
  bool operator<=(const tags&) const noexcept;
  ///\brief Tags are comparable.
  bool operator>=(const tags&) const noexcept;
  ///@}

  ///@{
  ///\brief Iterate over tags.
  iterator begin() const noexcept;
  ///\brief Iterate over tags.
  iterator end() const noexcept;
  ///@}

  /**
   * \brief Test if the tag set has all given key names defined.
   *
   * \tparam Iter An iterator type.
   * \param[in] b,e Iteration over tag names.
   * \return true if all names are defined in this tag set.
   */
  template<typename Iter> bool has_keys(Iter b, Iter e) const;

 private:
  map_type map_;
};

/**
 * \brief Write tags to output stream.
 * \ingroup intf_io
 * \relates tags
 *
 * \param out The output stream to write the tag set to.
 * \param t The tag set to write.
 * \return \p out
 */
monsoon_intf_export_
std::ostream& operator<<(std::ostream& out, const tags& t);

/**
 * \brief Get the string representation of the tag set.
 * \param t The tag set to create a string representation of.
 * \return String representation of the tag set.
 */
std::string to_string(const tags& t);


} /* namespace monsoon */


namespace std {


///\brief STL Hash support
///\ingroup intf_stl
///\relates tags
template<>
struct hash<monsoon::tags> {
  ///\brief Argument type for hash function.
  using argument_type = const monsoon::tags&;
  ///\brief Result type for hash function.
  using result_type = size_t;

  ///\brief Compute hash code for tags.
  monsoon_intf_export_
  size_t operator()(const monsoon::tags&) const noexcept;
};


} /* namespace std */

#include "tags-inl.h"

#endif /* MONSOON_TAGS_H */
