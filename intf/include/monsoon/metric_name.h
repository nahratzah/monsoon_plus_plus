#ifndef MONSOON_METRIC_NAME_H
#define MONSOON_METRIC_NAME_H

///\file
///\ingroup intf

#include <monsoon/intf_export_.h>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iosfwd>
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
 public:
  ///\brief The collection type holding path segments.
  using path_type = std::vector<std::string>;
  ///\brief Iterator over path segments.
  using iterator = path_type::iterator;
  ///\brief Const iterator over path segments.
  using const_iterator = path_type::const_iterator;

  metric_name() = default;
  metric_name(const metric_name&) = default;
  metric_name(metric_name&&) noexcept;
  metric_name& operator=(const metric_name&) = default;
  metric_name& operator=(metric_name&&) noexcept;

  /**
   * \brief Create a new metric name.
   *
   * \param path The path of the metric name.
   */
  explicit metric_name(path_type&& path) noexcept;
  /**
   * \brief Create a new metric name.
   *
   * \param path The path of the metric name.
   */
  explicit metric_name(const path_type& path);
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
  iterator begin();
  ///\brief Iterate over path segments.
  iterator end();
  ///\brief Iterate over path segments.
  const_iterator begin() const;
  ///\brief Iterate over path segments.
  const_iterator end() const;
  ///\brief Iterate over path segments.
  const_iterator cbegin() const;
  ///\brief Iterate over path segments.
  const_iterator cend() const;
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
  path_type path_;
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
