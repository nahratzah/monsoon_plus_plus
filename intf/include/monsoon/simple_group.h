#ifndef MONSOON_SIMPLE_GROUP_H
#define MONSOON_SIMPLE_GROUP_H

///\file
///\ingroup intf

#include <monsoon/intf_export_.h>
#include <monsoon/cache/allocator.h>
#include <monsoon/cache/cache.h>
#include <initializer_list>
#include <iosfwd>
#include <iterator>
#include <memory>
#include <string>
#include <vector>
#include <string_view>

namespace monsoon {


/**
 * \brief Simple group name.
 * \ingroup intf
 *
 * A simple group is simply the path of a group name.
 */
class monsoon_intf_export_ simple_group {
 private:
  using string_type = std::basic_string<char, std::char_traits<char>, cache_allocator<std::allocator<char>>>;

 public:
  ///\brief The internal path type.
  using path_type = std::vector<string_type, cache_allocator<std::allocator<string_type>>>;
  ///\brief Iterator over internal path.
  using iterator = path_type::const_iterator;

 private:
  struct cache_hasher_;
  struct cache_eq_;
  struct cache_create_;

  using cache_type = cache::extended_cache<
      void,
      const path_type,
      cache_hasher_,
      cache_eq_,
      cache_allocator<std::allocator<path_type>>,
      cache_create_>;

 public:
  ///\brief Default constructor creates an empty path.
  simple_group();

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

  /**
   * \brief Parse simple group expression.
   * \param s A string representation of a simple group path.
   * \return A simple group, as represented by the expression.
   * \throw std::invalid_argument indicating \p s does not hold a valid expression.
   */
  static simple_group parse(std::string_view s);

 private:
  /**
   * \brief Construct a simple group using input iterators.
   *
   * \tparam Iterator type.
   * \param[in] b,e Iterators over path segments.
   * \param[in] tag Iterator category of \p Iterator.
   */
  template<typename Iter> simple_group(Iter b, Iter e, std::input_iterator_tag tag);

  /**
   * \brief Construct a simple group using input iterators.
   *
   * \tparam Iterator type.
   * \param[in] b,e Iterators over path segments.
   * \param[in] tag Iterator category of \p Iterator.
   */
  template<typename Iter> simple_group(Iter b, Iter e, std::forward_iterator_tag tag);

  static cache_type cache_();
  std::shared_ptr<const path_type> path_;
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
