#ifndef MONSOON_SIMPLE_GROUP_INL_H
#define MONSOON_SIMPLE_GROUP_INL_H

#include <utility>

namespace monsoon {


inline simple_group::simple_group(simple_group&& other) noexcept
: path_(std::move(other.path_))
{}

inline auto simple_group::operator=(simple_group&& other) noexcept
->  simple_group& {
  path_ = std::move(other.path_);
  return *this;
}

inline simple_group::simple_group(std::initializer_list<const char*> init)
: path_(init.begin(), init.end())
{}

inline simple_group::simple_group(std::initializer_list<std::string> init)
: path_(init)
{}

template<typename Iter>
inline simple_group::simple_group(Iter b, Iter e)
: path_(b, e)
{}

inline auto simple_group::get_path() const noexcept -> const path_type& {
  return path_;
}

inline auto simple_group::begin() const noexcept -> iterator {
  return path_.begin();
}

inline auto simple_group::end() const noexcept -> iterator {
  return path_.end();
}

inline auto simple_group::operator!=(const simple_group& other) const noexcept
->  bool {
  return !(*this == other);
}

inline auto simple_group::operator>(const simple_group& other) const noexcept
->  bool {
  return other < *this;
}

inline auto simple_group::operator<=(const simple_group& other) const noexcept
->  bool {
  return !(other < *this);
}

inline auto simple_group::operator>=(const simple_group& other) const noexcept
->  bool {
  return !(*this < other);
}


} /* namespace monsoon */

#endif /* MONSOON_SIMPLE_GROUP_INL_H */
