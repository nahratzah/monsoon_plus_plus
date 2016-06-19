#ifndef MONSOON_TAGS_INL_H
#define MONSOON_TAGS_INL_H

#include <utility>

namespace monsoon {


inline tags::tags(tags&& other) noexcept
: map_(std::move(other.map_))
{}

inline auto tags::operator=(tags&& other) noexcept -> tags& {
  map_ = std::move(other.map_);
  return *this;
}

template<typename Iter>
tags::tags(Iter b, Iter e)
: map_(b, e)
{}

inline auto tags::empty() const noexcept -> bool {
  return map_.empty();
}

inline auto tags::get_map() const noexcept -> const map_type& {
  return map_;
}

inline auto tags::operator!=(const tags& other) const noexcept -> bool {
  return !(*this == other);
}


} /* namespace monsoon */

#endif /* MONSOON_TAGS_INL_H */
