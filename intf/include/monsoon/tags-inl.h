#ifndef MONSOON_TAGS_INL_H
#define MONSOON_TAGS_INL_H

#include <utility>

namespace monsoon {


template<typename Iter>
tags::tags(Iter b, Iter e)
: map_(b, e)
{}

inline tags::tags(map_type map) noexcept
: map_(std::move(map))
{}

inline tags::tags(std::initializer_list<map_type::value_type> il)
: map_(il)
{}

inline auto tags::empty() const noexcept -> bool {
  return map_.empty();
}

inline auto tags::get_map() const noexcept -> const map_type& {
  return map_;
}

inline auto tags::begin() const noexcept -> iterator {
  return map_.begin();
}

inline auto tags::end() const noexcept -> iterator {
  return map_.end();
}

inline auto tags::operator!=(const tags& other) const noexcept -> bool {
  return !(*this == other);
}

inline auto tags::operator>(const tags& other) const noexcept -> bool {
  return other < *this;
}

inline auto tags::operator<=(const tags& other) const noexcept -> bool {
  return !(other < *this);
}

inline auto tags::operator>=(const tags& other) const noexcept -> bool {
  return !(*this < other);
}

template<typename Iter>
auto tags::has_keys(Iter b, Iter e) const -> bool {
  while (b != e) {
    if (map_.find(*b++) == map_.end())
      return false;
  }
  return true;
}


} /* namespace monsoon */

#endif /* MONSOON_TAGS_INL_H */
