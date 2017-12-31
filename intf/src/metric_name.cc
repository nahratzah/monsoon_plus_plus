#include <monsoon/metric_name.h>
#include <monsoon/config_support.h>
#include <monsoon/grammar/intf/rules.h>
#include <algorithm>
#include <utility>
#include <ostream>
#include <sstream>

namespace monsoon {


auto metric_name::operator==(const metric_name& other) const noexcept -> bool {
  return path_ == other.path_;
}

auto metric_name::operator<(const metric_name& other) const noexcept -> bool {
  return std::lexicographical_compare(path_.begin(), path_.end(),
                                      other.path_.begin(), other.path_.end());
}

auto metric_name::config_string() const -> std::string {
  return (std::ostringstream() << *this).str();
}

metric_name metric_name::parse(std::string_view s) {
  std::string_view::iterator parse_end = s.begin();

  grammar::ast::simple_path_lit_expr result;
  bool r = grammar::x3::phrase_parse(
      parse_end, s.end(),
      grammar::parser::simple_path_lit,
      grammar::x3::space,
      result);
  if (r && parse_end == s.end())
    return result;
  throw std::invalid_argument("invalid expression");
}


auto operator<<(std::ostream& out, const metric_name& n) -> std::ostream& {
  bool first = true;

  for (const std::string& s : n.get_path()) {
    if (!std::exchange(first, false))
      out << ".";
    out << maybe_quote_identifier(s);
  }
  return out;
}


} /* namespace monsoon */


namespace std {


auto std::hash<monsoon::metric_name>::operator()(const monsoon::metric_name& v)
    const noexcept
->  size_t {
  auto s_hash = std::hash<std::string>();

  size_t rv = 0;
  for (const std::string& s : v.get_path())
    rv = 17u * rv + s_hash(s);
  return rv;
}


} /* namespace std */
