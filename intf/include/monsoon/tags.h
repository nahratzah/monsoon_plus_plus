#ifndef MONSOON_TAGS_H
#define MONSOON_TAGS_H

#include <monsoon/intf_export_.h>
#include <monsoon/metric_value.h>
#include <optional>
#include <cstddef>
#include <functional>
#include <iosfwd>
#include <string>
#include <map>

namespace monsoon {


class monsoon_intf_export_ tags {
 public:
  using map_type = std::map<std::string, metric_value>;
  using iterator = map_type::const_iterator;

  tags() = default;
  tags(const tags&) = default;
  tags(tags&&) noexcept;
  tags& operator=(const tags&) = default;
  tags& operator=(tags&&) noexcept;

  template<typename Iter> tags(Iter, Iter);
  tags(map_type) noexcept;
  tags(std::initializer_list<map_type::value_type>);

  bool empty() const noexcept;
  const map_type& get_map() const noexcept;
  auto operator[](const std::string&) const noexcept
      -> std::optional<metric_value>;

  bool operator==(const tags&) const noexcept;
  bool operator!=(const tags&) const noexcept;
  bool operator<(const tags&) const noexcept;
  bool operator>(const tags&) const noexcept;
  bool operator<=(const tags&) const noexcept;
  bool operator>=(const tags&) const noexcept;

  iterator begin() const noexcept;
  iterator end() const noexcept;

  std::string tag_string() const;
  template<typename Iter> bool has_keys(Iter, Iter) const;

 private:
  map_type map_;
};

monsoon_intf_export_
std::ostream& operator<<(std::ostream&, const tags&);


} /* namespace monsoon */


namespace std {


template<>
struct hash<monsoon::tags> {
  using argument_type = const monsoon::tags&;
  using result_type = size_t;

  monsoon_intf_export_
  size_t operator()(const monsoon::tags&) const noexcept;
};


} /* namespace std */

#include "tags-inl.h"

#endif /* MONSOON_TAGS_H */
