#ifndef MONSOON_PATH_COMMON_H
#define MONSOON_PATH_COMMON_H

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
 * \brief Common data structure for simple_group and metric_name.
 * \details The two types are completely similar and only differ
 * in meaning (type name). Therefore, they should share a common
 * base.
 */
class monsoon_intf_export_ path_common {
 public:
  ///\brief String type used by path types.
  using string_type = std::basic_string<char, std::char_traits<char>, cache_allocator<std::allocator<char>>>;
  ///\brief The internal path type.
  using path_type = std::vector<string_type, cache_allocator<std::allocator<string_type>>>;
  ///\brief Iterator over internal path.
  using const_iterator = path_type::const_iterator;
  ///\brief Iterator over internal path.
  using iterator = const_iterator;

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

 protected:
  ///\brief Default constructor creates an empty path.
  path_common();

  /**
   * \brief Construct a simple group using the supplied path.
   *
   * \param path A path.
   */
  explicit path_common(const path_type& path);
  /**
   * \brief Construct a simple group using the supplied path.
   *
   * \param path A path.
   */
  template<typename T, typename Alloc>
  explicit path_common(const std::vector<T, Alloc>& path);
  /**
   * \brief Construct a simple group using the supplied path.
   *
   * \param path A path.
   */
  path_common(std::initializer_list<const char*> path);
  /**
   * \brief Construct a simple group using the supplied path.
   *
   * \param path A path.
   */
  path_common(std::initializer_list<std::string> path);
  /**
   * \brief Construct a simple group using the supplied path.
   *
   * \param path A path.
   */
  path_common(std::initializer_list<std::string_view> path);
  /**
   * \brief Construct a simple group.
   *
   * \tparam Iterator type.
   * \param[in] b,e Iterators over path segments.
   */
  template<typename Iter> path_common(Iter b, Iter e);

  ~path_common() noexcept = default;

 public:
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

 protected:
  ///@{
  bool operator==(const path_common&) const noexcept;
  bool operator!=(const path_common&) const noexcept;
  bool operator<(const path_common&) const noexcept;
  bool operator>(const path_common&) const noexcept;
  bool operator<=(const path_common&) const noexcept;
  bool operator>=(const path_common&) const noexcept;
  ///@}

 public:
  /**
   * \brief Retrieve textual representation.
   *
   * \return Textual representation of this simple group.
   */
  std::string config_string() const;

 private:
  /**
   * \brief Construct a simple group using input iterators.
   *
   * \tparam Iterator type.
   * \param[in] b,e Iterators over path segments.
   * \param[in] tag Iterator category of \p Iterator.
   */
  template<typename Iter> path_common(Iter b, Iter e, std::input_iterator_tag tag);

  /**
   * \brief Construct a simple group using input iterators.
   *
   * \tparam Iterator type.
   * \param[in] b,e Iterators over path segments.
   * \param[in] tag Iterator category of \p Iterator.
   */
  template<typename Iter> path_common(Iter b, Iter e, std::forward_iterator_tag tag);

  static cache_type cache_();
  std::shared_ptr<const path_type> path_;
};

/**
 * \brief Write textual representation of a path to output stream.
 * \ingroup intf_io
 *
 * \param out Output stream.
 * \param n The path for which to write a textual representation.
 * \return \p out
 */
monsoon_intf_export_
std::ostream& operator<<(std::ostream& out, const path_common& n);


} /* namespace monsoon */


namespace std {


///\brief STL Hash support.
///\ingroup intf_stl
///\relates path_common
template<>
struct hash<monsoon::path_common> {
  ///\brief Argument type for hash function.
  using argument_type = const monsoon::path_common&;
  ///\brief Result type for hash function.
  using result_type = size_t;

  ///\brief Compute hash code for path_common.
  monsoon_intf_export_
  size_t operator()(const monsoon::path_common&) const noexcept;
};


} /* namespace std */

#include "path_common-inl.h"

#endif /* MONSOON_PATH_COMMON_H */
