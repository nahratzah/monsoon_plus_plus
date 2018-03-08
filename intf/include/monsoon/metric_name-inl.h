#ifndef MONSOON_METRIC_NAME_INL_H
#define MONSOON_METRIC_NAME_INL_H

namespace monsoon {


inline auto metric_name::from_path(const path_common& p) noexcept
-> metric_name {
  return metric_name(p);
}

inline auto metric_name::from_path(path_common&& p) noexcept
-> metric_name {
  return metric_name(std::move(p));
}

inline metric_name::metric_name(const path_type& p)
: path_common(p)
{}

inline metric_name::metric_name(std::initializer_list<const char*> init)
: path_common(init)
{}

inline metric_name::metric_name(std::initializer_list<std::string> init)
: path_common(init)
{}

inline metric_name::metric_name(std::initializer_list<std::string_view> init)
: path_common(init)
{}

template<typename T, typename Alloc>
inline metric_name::metric_name(const std::vector<T, Alloc>& p)
: path_common(p)
{}

template<typename Iter>
metric_name::metric_name(Iter b, Iter e)
: path_common(b, e)
{}

inline auto metric_name::operator==(const metric_name& other) const noexcept
->  bool {
  return this->path_common::operator==(other);
}

inline auto metric_name::operator!=(const metric_name& other) const noexcept
->  bool {
  return this->path_common::operator!=(other);
}

inline auto metric_name::operator<(const metric_name& other) const noexcept
->  bool {
  return this->path_common::operator<(other);
}

inline auto metric_name::operator>(const metric_name& other) const noexcept
->  bool {
  return this->path_common::operator>(other);
}

inline auto metric_name::operator<=(const metric_name& other) const noexcept
->  bool {
  return this->path_common::operator<=(other);
}

inline auto metric_name::operator>=(const metric_name& other) const noexcept
->  bool {
  return this->path_common::operator>=(other);
}

inline std::ostream& operator<<(std::ostream& out, const metric_name& n) {
  return out << static_cast<const path_common&>(n);
}


} /* namespace monsoon */


namespace std {


inline auto std::hash<monsoon::metric_name>::operator()(const monsoon::metric_name& v)
    const noexcept
->  size_t {
  return std::hash<monsoon::path_common>()(v);
}


} /* namespace std */

#endif /* MONSOON_METRIC_NAME_INL_H */
