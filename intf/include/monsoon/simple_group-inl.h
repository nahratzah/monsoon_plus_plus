#ifndef MONSOON_SIMPLE_GROUP_INL_H
#define MONSOON_SIMPLE_GROUP_INL_H

namespace monsoon {


inline auto simple_group::from_path(const path_common& p) noexcept
-> simple_group {
  return simple_group(p);
}

inline auto simple_group::from_path(path_common&& p) noexcept
-> simple_group {
  return simple_group(std::move(p));
}

inline simple_group::simple_group(const path_type& p)
: path_common(p)
{}

inline simple_group::simple_group(std::initializer_list<const char*> init)
: path_common(init)
{}

inline simple_group::simple_group(std::initializer_list<std::string> init)
: path_common(init)
{}

inline simple_group::simple_group(std::initializer_list<std::string_view> init)
: path_common(init)
{}

template<typename T, typename Alloc>
inline simple_group::simple_group(const std::vector<T, Alloc>& p)
: path_common(p)
{}

template<typename Iter>
inline simple_group::simple_group(Iter b, Iter e)
: path_common(b, e)
{}

inline auto simple_group::operator==(const simple_group& other) const noexcept
->  bool {
  return this->path_common::operator==(other);
}

inline auto simple_group::operator!=(const simple_group& other) const noexcept
->  bool {
  return this->path_common::operator!=(other);
}

inline auto simple_group::operator<(const simple_group& other) const noexcept
->  bool {
  return this->path_common::operator<(other);
}

inline auto simple_group::operator>(const simple_group& other) const noexcept
->  bool {
  return this->path_common::operator>(other);
}

inline auto simple_group::operator<=(const simple_group& other) const noexcept
->  bool {
  return this->path_common::operator<=(other);
}

inline auto simple_group::operator>=(const simple_group& other) const noexcept
->  bool {
  return this->path_common::operator>=(other);
}

inline std::ostream& operator<<(std::ostream& out, const simple_group& n) {
  return out << static_cast<const path_common&>(n);
}


} /* namespace monsoon */


namespace std {


inline auto std::hash<monsoon::simple_group>::operator()(const monsoon::simple_group& v)
    const noexcept
->  size_t {
  return std::hash<monsoon::path_common>()(v);
}


} /* namespace std */

#endif /* MONSOON_SIMPLE_GROUP_INL_H */
