#ifndef MONSOON_GROUP_NAME_H
#define MONSOON_GROUP_NAME_H

#include <monsoon/intf_export_.h>
#include <monsoon/simple_group.h>
#include <monsoon/tags.h>
#include <cstddef>
#include <functional>
#include <iosfwd>

namespace monsoon {


class monsoon_intf_export_ group_name {
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

  std::string config_string() const;

 private:
  simple_group path_;
  tags tags_;
};

std::ostream& operator<<(std::ostream&, const group_name&);


} /* namespace monsoon */


namespace std {


template<>
struct hash<monsoon::group_name> {
  using argument_type = const monsoon::group_name&;
  using result_type = size_t;

  monsoon_intf_export_
  size_t operator()(const monsoon::group_name&) const noexcept;
};


} /* namespace std */

#include "group_name-inl.h"

#endif /* MONSOON_GROUP_NAME_H */
