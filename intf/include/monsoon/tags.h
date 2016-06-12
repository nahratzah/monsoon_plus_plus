#ifndef MONSOON_TAGS_H
#define MONSOON_TAGS_H

#include <monsoon/metric_value.h>
#include <monsoon/optional.h>
#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>

namespace monsoon {


class tags {
 public:
  using map_type = std::unordered_map<std::string, metric_value>;

  tags() = default;
  tags(const tags&) = default;
  tags(tags&&) noexcept;
  tags& operator=(const tags&) = default;
  tags& operator=(tags&&) noexcept;

  template<typename Iter> tags(Iter, Iter);

  const map_type& get_map() const noexcept;
  optional<const metric_value&> operator[](const std::string&) const noexcept;

  bool operator==(const tags&) const noexcept;
  bool operator!=(const tags&) const noexcept;

 private:
  map_type map_;
};


} /* namespace monsoon */


namespace std {


template<>
struct hash<monsoon::tags> {
  using argument_type = const monsoon::tags&;
  using result_type = size_t;

  size_t operator()(const monsoon::tags&) const noexcept;
};


} /* namespace std */

#include "tags-inl.h"

#endif /* MONSOON_TAGS_H */
