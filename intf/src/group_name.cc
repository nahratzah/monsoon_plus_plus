#include <monsoon/group_name.h>
#include <monsoon/config_support.h>
#include <monsoon/grammar/intf/rules.h>
#include <utility>
#include <ostream>
#include <sstream>

namespace monsoon {


group_name group_name::parse(std::string_view s) {
  std::string_view::iterator parse_end = s.begin();

  grammar::ast::group_name_lit_expr result;
  bool r = grammar::x3::phrase_parse(
      parse_end, s.end(),
      grammar::parser::group_name_lit,
      grammar::x3::space,
      result);
  if (r && parse_end == s.end())
    return result;
  throw std::invalid_argument("invalid expression");
}

auto group_name::operator==(const group_name& other) const noexcept -> bool {
  return path_ == other.path_ && tags_ == other.tags_;
}

auto group_name::operator<(const group_name& other) const noexcept -> bool {
  return path_ < other.path_
      || (path_ == other.path_ && tags_ < other.tags_);
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
