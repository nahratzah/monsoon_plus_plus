#ifndef MONSOON_METRIC_NAME_H
#define MONSOON_METRIC_NAME_H

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

namespace monsoon {


/**
 * \brief The name of a metric.
 * \ingroup intf
 *
 * Metric names are always local to a group.
 */
class monsoon_intf_export_ metric_name {
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
  metric_name();

  /**
   * \brief Create a new metric name.
   *
   * \param path The path of the metric name.
   */
  explicit metric_name(const path_type& path);
  /**
   * \brief Construct a simple group using the supplied path.
   *
   * \param path The path of the constructed simple group.
   */
  template<typename T, typename Alloc>
  explicit metric_name(const std::vector<T, Alloc>& path);
  /**
   * \brief Create a new metric name.
   *
   * \param path The path of the metric name.
   */
  metric_name(std::initializer_list<const char*> path);
  /**
   * \brief Create a new metric name.
   *
   * \param path The path of the metric name.
   */
  metric_name(std::initializer_list<std::string> path);
  /**
   * \brief Create a new metric name.
   *
   * \param[in] b,e Iteration of path segments.
   */
  template<typename Iter> metric_name(Iter b, Iter e);

  /**
   * @return the path of this metric name.
   */
  const path_type& get_path() const noexcept;

  ///@{
  ///\brief Iterate over path segments.
  iterator begin() const noexcept;
  ///\brief Iterate over path segments.
  iterator end() const noexcept;
  ///@}

  ///@{
  ///\brief Comparison.
  bool operator==(const metric_name&) const noexcept;
  ///\brief Comparison.
  bool operator!=(const metric_name&) const noexcept;
  ///\brief Comparison.
  bool operator<(const metric_name&) const noexcept;
  ///\brief Comparison.
  bool operator>(const metric_name&) const noexcept;
  ///\brief Comparison.
  bool operator<=(const metric_name&) const noexcept;
  ///\brief Comparison.
  bool operator>=(const metric_name&) const noexcept;
  ///@}

  /**
   * \brief The textual representation of the metric name.
   */
  std::string config_string() const;

  /**
   * \brief Parse metric name.
   * \param s A string representation of a metric name.
   * \return A metric name, as represented by the expression.
   * \throw std::invalid_argument indicating \p s does not hold a valid expression.
   */
  static metric_name parse(std::string_view s);

 private:
  /**
   * \brief Construct a simple group using input iterators.
   *
   * \tparam Iterator type.
   * \param[in] b,e Iterators over path segments.
   * \param[in] tag Iterator category of \p Iterator.
   */
  template<typename Iter> metric_name(Iter b, Iter e, std::input_iterator_tag tag);

  /**
   * \brief Construct a simple group using input iterators.
   *
   * \tparam Iterator type.
   * \param[in] b,e Iterators over path segments.
   * \param[in] tag Iterator category of \p Iterator.
   */
  template<typename Iter> metric_name(Iter b, Iter e, std::forward_iterator_tag tag);

  static cache_type cache_();
  std::shared_ptr<const path_type> path_;
};

/**
 * \brief Write metric name to a stream.
 * \ingroup intf_io
 *
 * @param n The metric name of which to write the textual representation.
 * @param out The destination stream.
 * @return out
 */
monsoon_intf_export_
std::ostream& operator<<(std::ostream& out, const metric_name& n);


} /* namespace monsoon */


namespace std {


///\brief STL Hash support.
///\ingroup intf_stl
///\relates metric_name
template<>
struct hash<monsoon::metric_name> {
  ///\brief Argument type for hash function.
  using argument_type = const monsoon::metric_name&;
  ///\brief Result type for hash function.
  using result_type = size_t;

  ///\brief Compute hash code for metric_name.
  monsoon_intf_export_
  size_t operator()(const monsoon::metric_name&) const noexcept;
};


} /* namespace std */

#include "metric_name-inl.h"

#endif /* MONSOON_METRIC_NAME_H */
