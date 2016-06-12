#include <monsoon/group_name.h>

namespace monsoon {


auto group_name::operator==(const group_name& other) const noexcept -> bool {
  return path_ == other.path_ && tags_ == other.tags_;
}


} /* namespace monsoon */


namespace std {


auto std::hash<monsoon::simple_group>::operator(const monsoon::simple_group& v)
    const noexcept
->  size_t {
  return std::hash<monsoon::simple_group>()(v.get_path()) ^
         std::hash<monsoon::tags>()(v.get_tags());
}


} /* namespace std */
