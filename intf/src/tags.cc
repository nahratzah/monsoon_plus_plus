#include <monsoon/tags.h>
#include <monsoon/hash_support.h>
#include <monsoon/config_support.h>
#include <monsoon/grammar/intf/rules.h>
#include <algorithm>
#include <utility>
#include <ostream>
#include <sstream>

namespace monsoon {


tags tags::parse(std::string_view s) {
  std::string_view::iterator parse_end = s.begin();

  grammar::ast::tags_lit_expr result;
  bool r = grammar::x3::phrase_parse(
      parse_end, s.end(),
      grammar::parser::tags_lit,
      grammar::x3::space,
      result);
  if (r && parse_end == s.end())
    return result;
  throw std::invalid_argument("invalid expression");
}

auto tags::operator[](const std::string& key) const noexcept
->  std::optional<metric_value> {
  auto pos = map_.find(key);
  if (pos == map_.end()) return {};
  return pos->second;
}

auto tags::operator==(const tags& other) const noexcept -> bool {
  return map_ == other.map_;
}

auto tags::operator<(const tags& other) const noexcept -> bool {
  return std::lexicographical_compare(
      map_.begin(), map_.end(),
      other.map_.begin(), other.map_.end(),
      [](map_type::const_reference x, map_type::const_reference y) {
        if (std::get<0>(x) != std::get<0>(y)) return std::get<0>(x) < std::get<0>(y);
        return metric_value::before(std::get<1>(x), std::get<1>(y));
      });
}


auto operator<<(std::ostream& out, const tags& tv) -> std::ostream& {
  if (tv.empty()) return out << "{}";

  bool first = true;
  out << "{";
  for (const auto& s : tv.get_map()) {
    if (!std::exchange(first, false))
      out << ", ";
    out << maybe_quote_identifier(std::get<0>(s)) << "=" << std::get<1>(s);
  }
  out << "}";
  return out;
}

auto to_string(const tags& t) -> std::string {
  return (std::ostringstream() << t).str();
}


} /* namespace monsoon */


namespace std {


auto hash<monsoon::tags>::operator()(const monsoon::tags& v) const noexcept
->  size_t {
  return monsoon::map_to_hash(v.get_map());
}


} /* namespace std */
