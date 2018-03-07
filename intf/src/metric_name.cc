#include <monsoon/metric_name.h>
#include <monsoon/config_support.h>
#include <monsoon/grammar/intf/rules.h>
#include <monsoon/cache/impl.h>
#include <algorithm>
#include <utility>
#include <ostream>
#include <sstream>
#include <chrono>

namespace monsoon {


metric_name::cache_type metric_name::cache_() {
  static cache_type impl = metric_name::cache_type::builder()
      .access_expire(std::chrono::minutes(10))
      .build(cache_create_());
  return impl;
}

metric_name::metric_name()
: path_(cache_()())
{}

metric_name::metric_name(const path_type& p)
: path_(cache_()(p))
{}

metric_name::metric_name(std::initializer_list<const char*> init)
: metric_name(init.begin(), init.end())
{}

metric_name::metric_name(std::initializer_list<std::string> init)
: metric_name(init.begin(), init.end())
{}

metric_name::metric_name(std::initializer_list<std::string_view> init)
: metric_name(init.begin(), init.end())
{}

auto metric_name::operator==(const metric_name& other) const noexcept -> bool {
  return path_ == other.path_
      || std::equal(begin(), end(), other.begin(), other.end());
}

auto metric_name::operator<(const metric_name& other) const noexcept -> bool {
  return path_ != other.path_
      && std::lexicographical_compare(begin(), end(),
                                      other.begin(), other.end());
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

  for (std::string_view s : n.get_path()) {
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
  auto s_hash = std::hash<std::string_view>();

  size_t rv = 0;
  for (std::string_view s : v.get_path())
    rv = 17u * rv + s_hash(s);
  return rv;
}


} /* namespace std */
