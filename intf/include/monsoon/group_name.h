#ifndef MONSOON_GROUP_NAME_H
#define MONSOON_GROUP_NAME_H

///\file
///\ingroup intf

#include <monsoon/intf_export_.h>
#include <monsoon/simple_group.h>
#include <monsoon/tags.h>
#include <cstddef>
#include <functional>
#include <iosfwd>

namespace monsoon {


/**
 * \brief A group name.
 * \ingroup intf
 *
 * Group names consist of a path and a tag set.
 */
class monsoon_intf_export_ group_name {
 public:
  group_name() = default;
  group_name(const group_name&) = default;
  group_name(group_name&&) noexcept;
  group_name& operator=(const group_name&) = default;
  group_name& operator=(group_name&&) noexcept;

  /**
   * \brief Construct a group with an empty tag set.
   *
   * \param path The path of the group.
   */
  explicit group_name(simple_group path);
  /**
   * \brief Construct a group.
   *
   * \param path The path of the group.
   * \param tagset The tag set of the group.
   */
  group_name(simple_group path, tags tagset) noexcept;

  /**
   * \brief Parse group name expression.
   * \param s A string representation of a group name.
   *   The tag set may be omitted.
   * \return A group name, as represented by the expression.
   * \throw std::invalid_argument indicating \p s does not hold a valid expression.
   */
  static group_name parse(std::string_view s);

  ///@{
  ///\brief Get the path of the group.
  const simple_group& get_path() const noexcept;
  ///\brief Get the tags portion of the group.
  const tags& get_tags() const noexcept;
  ///@}

  ///@{
  bool operator==(const group_name&) const noexcept;
  bool operator!=(const group_name&) const noexcept;
  bool operator<(const group_name&) const noexcept;
  bool operator>(const group_name&) const noexcept;
  bool operator<=(const group_name&) const noexcept;
  bool operator>=(const group_name&) const noexcept;
  ///@}

  /**
   * \brief Retrieve the textual representation of the group name.
   */
  std::string config_string() const;

 private:
  simple_group path_;
  tags tags_;
};

/**
 * \brief Write textual representation of the group name to a stream.
 * \ingroup intf_io
 *
 * \param out The output stream.
 * \param n The group name to write out.
 * \return \p out
 */
monsoon_intf_export_
std::ostream& operator<<(std::ostream& out, const group_name& n);


} /* namespace monsoon */


namespace std {


///\brief STL Hash support.
///\ingroup intf_stl
///\relates group_name
template<>
struct hash<monsoon::group_name> {
  ///\brief Argument type for hash function.
  using argument_type = const monsoon::group_name&;
  ///\brief Result type for hash function.
  using result_type = size_t;

  ///\brief Compute hash code for group_name.
  monsoon_intf_export_
  size_t operator()(const monsoon::group_name&) const noexcept;
};


} /* namespace std */

#include "group_name-inl.h"

#endif /* MONSOON_GROUP_NAME_H */
