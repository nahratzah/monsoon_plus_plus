#include <monsoon/simple_group.h>
#include <monsoon/config_support.h>
#include <monsoon/grammar/intf/rules.h>
#include <monsoon/cache/impl.h>
#include <algorithm>
#include <utility>
#include <ostream>
#include <sstream>
#include <chrono>

namespace monsoon {


simple_group::cache_type simple_group::cache_() {
  static cache_type impl = simple_group::cache_type::builder()
      .access_expire(std::chrono::minutes(10))
      .build(cache_create_());
  return impl;
}

auto simple_group::operator==(const simple_group& other) const noexcept
->  bool {
  return path_ == other.path_ ||
      std::equal(begin(), end(), other.begin(), other.end());
}

auto simple_group::operator<(const simple_group& other) const noexcept
->  bool {
  return path_ != other.path_
      && std::lexicographical_compare(begin(), end(),
                                      other.begin(), other.end());
}

auto simple_group::config_string() const -> std::string {
  return (std::ostringstream() << *this).str();
}

simple_group simple_group::parse(std::string_view s) {
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


auto operator<<(std::ostream& out, const simple_group& n) -> std::ostream& {
  bool first = true;

  for (std::string_view s : n) {
    if (!std::exchange(first, false))
      out << ".";
    out << maybe_quote_identifier(s);
  }
  return out;
}


} /* namespace monsoon */


namespace std {


auto std::hash<monsoon::simple_group>::operator()(const monsoon::simple_group& v)
    const noexcept
->  size_t {
  auto s_hash = std::hash<std::string_view>();

  size_t rv = 0;
  for (std::string_view s : v)
    rv = 19u * rv + s_hash(s);
  return rv;
}


} /* namespace std */
