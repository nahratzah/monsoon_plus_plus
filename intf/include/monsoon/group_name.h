#ifndef MONSOON_GROUP_NAME_H
#define MONSOON_GROUP_NAME_H

#include <monsoon/simple_group.h>
#include <monsoon/tags.h>
#include <cstddef>
#include <functional>

namespace monsoon {


class group_name {
 public:
  group_name() = default;
  group_name(const group_name&) = default;
  group_name(group_name&&) noexcept;
  group_name& operator=(const group_name&) = default;
  group_name& operator=(group_name&&) noexcept;

  group_name(simple_group);
  group_name(simple_group, tags) noexcept;

  const simple_group& get_path() const noexcept;
  const tags& get_tags() const noexcept;

  bool operator==(const group_name&) const noexcept;
  bool operator!=(const group_name&) const noexcept;

 private:
  simple_group path_;
  tags tags_;
};


} /* namespace monsoon */


namespace std {


template<>
struct hash<monsoon::group_name> {
  using argument_type = const monsoon::group_name&;
  using result_type = size_t;

  size_t operator()(const monsoon::group_name&) const noexcept;
};


} /* namespace std */

#include "group_name-inl.h"

#endif /* MONSOON_GROUP_NAME_H */
