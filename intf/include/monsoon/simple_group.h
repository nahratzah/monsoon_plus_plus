#ifndef MONSOON_SIMPLE_GROUP_H
#define MONSOON_SIMPLE_GROUP_H

///\file
///\ingroup intf

#include <monsoon/intf_export_.h>
#include <monsoon/path_common.h>

namespace monsoon {


/**
 * \brief Simple group name.
 * \ingroup intf
 *
 * A simple group is simply the path of a group name.
 */
class monsoon_intf_export_ simple_group
: public path_common
{
 public:
  ///\brief Default constructor creates an empty path.
  simple_group() = default;

 private:
  explicit simple_group(const path_common& p) noexcept : path_common(p) {}
  explicit simple_group(path_common&& p) noexcept : path_common(std::move(p)) {}

 public:
  ///\brief Construct simple group from common path.
  static auto from_path(const path_common& p) noexcept -> simple_group;
  ///\brief Construct simple group from common path.
  static auto from_path(path_common&& p) noexcept -> simple_group;

  /**
   * \brief Construct a simple group using the supplied path.
   *
   * \param path The path of the constructed simple group.
   */
  explicit simple_group(const path_type& path);
  /**
   * \brief Construct a simple group using the supplied path.
   *
   * \param path The path of the constructed simple group.
   */
  template<typename T, typename Alloc>
  explicit simple_group(const std::vector<T, Alloc>& path);
  /**
   * \brief Construct a simple group using the supplied path.
   *
   * \param path The path of the constructed simple group.
   */
  simple_group(std::initializer_list<const char*> path);
  /**
   * \brief Construct a simple group using the supplied path.
   *
   * \param path The path of the constructed simple group.
   */
  simple_group(std::initializer_list<std::string> path);
  /**
   * \brief Construct a simple group using the supplied path.
   *
   * \param path The path of the constructed simple group.
   */
  simple_group(std::initializer_list<std::string_view> path);
  /**
   * \brief Construct a simple group.
   *
   * \tparam Iterator type.
   * \param[in] b,e Iterators over path segments.
   */
  template<typename Iter> simple_group(Iter b, Iter e);

  ///@{
  bool operator==(const simple_group&) const noexcept;
  bool operator!=(const simple_group&) const noexcept;
  bool operator<(const simple_group&) const noexcept;
  bool operator>(const simple_group&) const noexcept;
  bool operator<=(const simple_group&) const noexcept;
  bool operator>=(const simple_group&) const noexcept;
  ///@}

  /**
   * \brief Parse simple group expression.
   * \param s A string representation of a simple group path.
   * \return A simple group, as represented by the expression.
   * \throw std::invalid_argument indicating \p s does not hold a valid expression.
   */
  static simple_group parse(std::string_view s);
};


} /* namespace monsoon */


namespace std {


///\brief STL Hash support.
///\ingroup intf_stl
///\relates simple_group
template<>
struct hash<monsoon::simple_group> {
  ///\brief Argument type for hash function.
  using argument_type = const monsoon::simple_group&;
  ///\brief Result type for hash function.
  using result_type = size_t;

  ///\brief Compute hash code for simple_group.
  size_t operator()(const monsoon::simple_group&) const noexcept;
};


} /* namespace std */

#include "simple_group-inl.h"

#endif /* MONSOON_SIMPLE_GROUP_H */
