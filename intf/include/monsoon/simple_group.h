#ifndef MONSOON_SIMPLE_GROUP_H
#define MONSOON_SIMPLE_GROUP_H

///\file
///\ingroup intf

#include <monsoon/intf_export_.h>
#include <initializer_list>
#include <iosfwd>
#include <string>
#include <vector>

namespace monsoon {


/**
 * \brief Simple group name.
 * \ingroup intf
 *
 * A simple group is simply the path of a group name.
 */
class monsoon_intf_export_ simple_group {
 public:
  ///\brief The internal path type.
  using path_type = std::vector<std::string>;
  ///\brief Iterator over internal path.
  using iterator = path_type::const_iterator;

  simple_group() = default;
  simple_group(const simple_group&) = default;
  simple_group(simple_group&&) noexcept;
  simple_group& operator=(const simple_group&) = default;
  simple_group& operator=(simple_group&&) noexcept;

  /**
   * \brief Construct a simple group using the supplied path.
   *
   * \param path The path of the constructed simple group.
   */
  explicit simple_group(path_type&& path) noexcept;
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
  simple_group(std::initializer_list<const char*> path);
  /**
   * \brief Construct a simple group using the supplied path.
   *
   * \param path The path of the constructed simple group.
   */
  simple_group(std::initializer_list<std::string> path);
  /**
   * \brief Construct a simple group.
   *
   * \tparam Iterator type.
   * \param[in] b,e Iterators over path segments.
   */
  template<typename Iter> simple_group(Iter b, Iter e);

  /**
   * \brief Get the underlying path.
   */
  const path_type& get_path() const noexcept;

  ///@{
  ///\brief Iterate over path elements.
  iterator begin() const noexcept;
  ///\brief Iterate over path elements.
  iterator end() const noexcept;
  ///@}

  ///@{
  bool operator==(const simple_group&) const noexcept;
  bool operator!=(const simple_group&) const noexcept;
  bool operator<(const simple_group&) const noexcept;
  bool operator>(const simple_group&) const noexcept;
  bool operator<=(const simple_group&) const noexcept;
  bool operator>=(const simple_group&) const noexcept;
  ///@}

  /**
   * \brief Retrieve textual representation.
   *
   * \return Textual representation of this simple group.
   */
  std::string config_string() const;

 private:
  path_type path_;
};

/**
 * \brief Write textual representation of simple group to output stream.
 * \ingroup intf_io
 *
 * \param out Output stream.
 * \param n The simple path for which to write a textual representation.
 * \return \p out
 */
monsoon_intf_export_
std::ostream& operator<<(std::ostream& out, const simple_group& n);


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
  monsoon_intf_export_
  size_t operator()(const monsoon::simple_group&) const noexcept;
};


} /* namespace std */

#include "simple_group-inl.h"

#endif /* MONSOON_SIMPLE_GROUP_H */
