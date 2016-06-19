#include <monsoon/group_name.h>
#include <monsoon/config_support.h>
#include <utility>
#include <ostream>
#include <sstream>

namespace monsoon {


auto group_name::operator==(const group_name& other) const noexcept -> bool {
  return path_ == other.path_ && tags_ == other.tags_;
}

auto group_name::config_string() const -> std::string {
  return (std::ostringstream() << *this).str();
}


auto operator<<(std::ostream& out, const group_name& n) -> std::ostream& {
  return out << n.get_path() << n.get_tags();
}


} /* namespace monsoon */


namespace std {


auto std::hash<monsoon::group_name>::operator()(const monsoon::group_name& v)
    const noexcept
->  size_t {
  return std::hash<monsoon::simple_group>()(v.get_path()) ^
         std::hash<monsoon::tags>()(v.get_tags());
}


} /* namespace std */
