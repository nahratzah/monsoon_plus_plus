#ifndef MONSOON_METRIC_NAME_H
#define MONSOON_METRIC_NAME_H

///\file
///\ingroup intf

#include <monsoon/intf_export_.h>
#include <monsoon/path_common.h>

namespace monsoon {


/**
 * \brief The name of a metric.
 * \ingroup intf
 *
 * Metric names are always local to a group.
 */
class monsoon_intf_export_ metric_name
: public path_common
{
 public:
  ///\brief Default constructor creates an empty path.
  metric_name() = default;

 private:
  struct explicit_pc {};
  explicit metric_name(const path_common& p, [[maybe_unused]] explicit_pc pc) noexcept : path_common(p) {}
  explicit metric_name(path_common&& p, [[maybe_unused]] explicit_pc pc) noexcept : path_common(std::move(p)) {}

 public:
  ///\brief Construct metric name from common path.
  static auto from_path(const path_common& p) noexcept -> metric_name;
  ///\brief Construct metric name from common path.
  static auto from_path(path_common&& p) noexcept -> metric_name;

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
   * \param path The path of the metric name.
   */
  metric_name(std::initializer_list<std::string_view> path);
  /**
   * \brief Create a new metric name.
   *
   * \param[in] b,e Iteration of path segments.
   */
  template<typename Iter> metric_name(Iter b, Iter e);

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
   * \brief Parse metric name.
   * \param s A string representation of a metric name.
   * \return A metric name, as represented by the expression.
   * \throw std::invalid_argument indicating \p s does not hold a valid expression.
   */
  static metric_name parse(std::string_view s);
};


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
  size_t operator()(const monsoon::metric_name&) const noexcept;
};


} /* namespace std */

#include "metric_name-inl.h"

#endif /* MONSOON_METRIC_NAME_H */
