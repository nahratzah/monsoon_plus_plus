#ifndef MONSOON_METRIC_NAME_INL_H
#define MONSOON_METRIC_NAME_INL_H

#include <utility>

namespace monsoon {


inline metric_name::metric_name(metric_name&& other) noexcept
: path_(std::move(other.path_))
{}

inline auto metric_name::operator=(metric_name&& other) noexcept
->  metric_name& {
  path_ = std::move(other.path_);
  return *this;
}

inline metric_name::metric_name(path_type&& p) noexcept
: path_(std::move(p))
{}

inline metric_name::metric_name(const path_type& p)
: path_(p)
{}

inline metric_name::metric_name(std::initializer_list<const char*> init)
: path_(init.begin(), init.end())
{}

inline metric_name::metric_name(std::initializer_list<std::string> init)
: path_(init)
{}

template<typename Iter>
metric_name::metric_name(Iter b, Iter e)
: path_(b, e)
{}

inline auto metric_name::get_path() const noexcept -> const path_type& {
  return path_;
}

inline auto metric_name::begin() -> iterator {
  return path_.begin();
}

inline auto metric_name::end() -> iterator {
  return path_.end();
}

inline auto metric_name::begin() const -> const_iterator {
  return path_.begin();
}

inline auto metric_name::end() const -> const_iterator {
  return path_.end();
}

inline auto metric_name::cbegin() const -> const_iterator {
  return path_.begin();
}

inline auto metric_name::cend() const -> const_iterator {
  return path_.end();
}

inline auto metric_name::operator!=(const metric_name& other) const noexcept
->  bool {
  return !(*this == other);
}

inline auto metric_name::operator>(const metric_name& other) const noexcept
->  bool {
  return other < *this;
}

inline auto metric_name::operator<=(const metric_name& other) const noexcept
->  bool {
  return !(other < *this);
}

inline auto metric_name::operator>=(const metric_name& other) const noexcept
->  bool {
  return !(*this < other);
}


} /* namespace monsoon */

#endif /* MONSOON_METRIC_NAME_INL_H */
