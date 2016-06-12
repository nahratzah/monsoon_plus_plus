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
