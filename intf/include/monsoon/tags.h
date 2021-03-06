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
#include <memory>
#include <monsoon/cache/allocator.h>
#include <monsoon/cache/cache.h>

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
  ///\brief Key type.
  using string_type = std::basic_string<char, std::char_traits<char>, cache_allocator<std::allocator<char>>>;
 private:

  ///\brief Underlying map type.
  using map_type = std::vector<
      std::pair<string_type, metric_value>,
      cache_allocator<std::allocator<std::pair<string_type, metric_value>>>>;

 public:
  ///\brief Iterator type.
  using const_iterator = map_type::const_iterator;
  using iterator = const_iterator;

 private:
  struct less_;
  struct cache_hasher_;
  struct cache_eq_;
  struct cache_create_;

  using cache_type = cache::extended_cache<
      void,
      const map_type,
      cache_hasher_,
      cache_eq_,
      cache_allocator<std::allocator<map_type>>,
      cache_create_>;

 public:
  ///@{
  ///\brief Create empty tag map.
  tags();

  /**
   * \brief Construct a tag set using an iteration.
   *
   * \tparam Iter Iterator type.
   * \param[in] b,e The iteration range to use to create the tag set.
   */
  template<typename Iter> tags(Iter b, Iter e);
  /**
   * \brief Construct a tag set using the values in the given \p collection.
   * \param[in] collection A list of key-value pairs from which to construct tags.
   */
  template<typename Collection>
  explicit tags(const Collection& collection);
  /**
   * \brief Construct a tag set using the given values.
   */
  explicit tags(const map_type& map);
  /**
   * \brief Construct a tag set using the given values.
   */
  explicit tags(map_type&& map);
  /**
   * \brief Construct a tag set using the given values.
   */
  tags(std::initializer_list<std::pair<std::string_view, metric_value>>);
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
   * \return the size of the tag set.
   */
  std::size_t size() const noexcept;
  /**
   * \brief Lookup tag value.
   * \param name The tag name to look for.
   * \return
   *   the metric value for the given tag name,
   *   or an empty optional if it does not exist.
   */
  auto operator[](std::string_view name) const noexcept
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
  const_iterator begin() const noexcept;
  ///\brief Iterate over tags.
  const_iterator end() const noexcept;
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
  template<typename Iter> tags(Iter b, Iter e, std::input_iterator_tag tag);
  template<typename Iter> tags(Iter b, Iter e, std::forward_iterator_tag tag);

  static const_iterator find_(const map_type& m, std::string_view v) noexcept;
  static void fix_and_validate_(map_type& m);

  static cache_type cache_();
  std::shared_ptr<const map_type> map_;
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
monsoon_intf_export_
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
