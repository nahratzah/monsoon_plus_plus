#ifndef MONSOON_GROUP_NAME_INL_H
#define MONSOON_GROUP_NAME_INL_H

#include <utility>

namespace monsoon {


inline group_name::group_name(group_name&& other) noexcept
: path_(std::move(other.path_)),
  tags_(std::move(other.tags_))
{}

inline auto group_name::operator=(group_name&& other) noexcept -> group_name& {
  path_ = std::move(other.path_);
  tags_ = std::move(other.tags_);
  return *this;
}

inline auto group_name::get_path() const noexcept -> const simple_group& {
  return path_;
}

inline auto group_name::get_tags() const noexcept -> const tags& {
  return tags_;
}

inline auto group_name::operator!=(const group_name& other) const noexcept
->  bool {
  return !(*this == other);
}


} /* namespace monsoon */

#endif /* MONSOON_GROUP_NAME_INL_H */
