#ifndef MONSOON_METRIC_NAME_H
#define MONSOON_METRIC_NAME_H

#include <monsoon/intf_export_.h>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iosfwd>
#include <string>
#include <vector>

namespace monsoon {


class monsoon_intf_export_ metric_name {
 public:
  using path_type = std::vector<std::string>;

  metric_name() = default;
  metric_name(const metric_name&) = default;
  metric_name(metric_name&&) noexcept;
  metric_name& operator=(const metric_name&) = default;
  metric_name& operator=(metric_name&&) noexcept;

  metric_name(std::initializer_list<const char*>);
  metric_name(std::initializer_list<std::string>);
  template<typename Iter> metric_name(Iter, Iter);

  const path_type& get_path() const noexcept;

  bool operator==(const metric_name&) const noexcept;
  bool operator!=(const metric_name&) const noexcept;
  bool operator<(const metric_name&) const noexcept;
  bool operator>(const metric_name&) const noexcept;
  bool operator<=(const metric_name&) const noexcept;
  bool operator>=(const metric_name&) const noexcept;

  std::string config_string() const;

 private:
  path_type path_;
};

monsoon_intf_export_
std::ostream& operator<<(std::ostream&, const metric_name&);


} /* namespace monsoon */


namespace std {


template<>
struct hash<monsoon::metric_name> {
  using argument_type = const monsoon::metric_name&;
  using result_type = size_t;

  monsoon_intf_export_
  size_t operator()(const monsoon::metric_name&) const noexcept;
};


} /* namespace std */

#include "metric_name-inl.h"

#endif /* MONSOON_METRIC_NAME_H */
