#include <monsoon/metric_name.h>
#include <algorithm>

namespace monsoon {


auto metric_name::operator==(const metric_name& other) const noexcept -> bool {
  return path_ == other.path_;
}

auto metric_name::operator<(const metric_name& other) const noexcept -> bool {
  return std::lexicographical_compare(path_.begin(), path_.end(),
                                      other.path_.begin(), other.path_.end());
}


} /* namespace monsoon */


namespace std {


auto std::hash<monsoon::metric_name>::operator(const monsoon::metric_name& v)
    const noexcept
->  size_t {
  auto s_hash = std::hash<std::string>();

  size_t rv = 0;
  for (const std::string& s : v.get_path())
    rv = 17u * rv + s_hash(s);
  return rv;
}


} /* namespace std */
