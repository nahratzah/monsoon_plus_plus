#include <monsoon/simple_group.h>
#include <algorithm>

namespace monsoon {


auto simple_group::operator==(const simple_group& other) const noexcept
->  bool {
  return path_ == other.path_;
}

auto simple_group::operator<(const simple_group& other) const noexcept
->  bool {
  return std::lexicographical_compare(path_.begin(), path_.end(),
                                      other.path_.begin(), other.path_.end());
}


} /* namespace monsoon */


namespace std {


auto std::hash<monsoon::simple_group>::operator()(const monsoon::simple_group& v)
    const noexcept
->  size_t {
  auto s_hash = std::hash<std::string>();

  size_t rv = 0;
  for (const std::string& s : v.get_path())
    rv = 19u * rv + s_hash(s);
  return rv;
}


} /* namespace std */
