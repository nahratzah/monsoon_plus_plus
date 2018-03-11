#include <monsoon/tags.h>
#include <monsoon/hash_support.h>
#include <monsoon/config_support.h>
#include <monsoon/cache/impl.h>
#include <monsoon/grammar/intf/rules.h>
#include <algorithm>
#include <utility>
#include <ostream>
#include <sstream>

namespace monsoon {


tags::cache_type tags::cache_() {
  static cache_type impl = tags::cache_type::builder()
      .access_expire(std::chrono::minutes(10))
      .build(cache_create_());
  return impl;
}

tags::tags()
: map_(cache_()())
{}

tags::tags(const map_type& map)
: map_(cache_()(map))
{}

tags::tags(map_type&& map)
: map_(cache_()(std::move(map)))
{}

tags::tags(std::initializer_list<std::pair<std::string_view, metric_value>> il)
: map_(cache_()(il.begin(), il.end()))
{}

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

auto tags::operator[](std::string_view key) const noexcept
->  std::optional<metric_value> {
  assert(map_ != nullptr);
  auto pos = find_(*map_, key);
  if (pos == map_->end()) return {};
  return pos->second;
}

auto tags::operator==(const tags& other) const noexcept -> bool {
  assert(map_ != nullptr && other.map_ != nullptr);
  return map_ == other.map_
      || *map_ == *other.map_;
}

auto tags::operator<(const tags& other) const noexcept -> bool {
  assert(map_ != nullptr && other.map_ != nullptr);
  return map_ != other.map_
      && std::lexicographical_compare(
          begin(), end(),
          other.begin(), other.end(),
          [](map_type::const_reference x, map_type::const_reference y) {
            if (std::get<0>(x) != std::get<0>(y)) return std::get<0>(x) < std::get<0>(y);
            return metric_value::before(std::get<1>(x), std::get<1>(y));
          });
}

auto tags::find_(const map_type& m, std::string_view v) noexcept
-> const_iterator {
  auto pos = std::lower_bound(m.begin(), m.end(), v, less_());
  if (pos != m.end() && pos->first != v) pos = m.end();
  return pos;
}

auto tags::fix_and_validate_(map_type& m)
-> void {
  std::sort(m.begin(), m.end(), less_());
  if (std::unique(m.begin(), m.end(),
          [](const map_type::value_type& x, const map_type::value_type& y) {
            return x.first == y.first;
          }) != m.end())
    throw std::invalid_argument("duplicate key in tags");
  if (std::any_of(m.begin(), m.end(),
          [](const map_type::value_type& e) {
            const auto& v = e.second.get();
            return std::holds_alternative<metric_value::empty>(v)
                || std::holds_alternative<histogram>(v);
          }))
    throw std::invalid_argument("empty or histogram metric value is not allowed in tags");
}


auto operator<<(std::ostream& out, const tags& tv) -> std::ostream& {
  if (tv.empty()) return out << "{}";

  bool first = true;
  out << "{";
  for (const auto& s : tv) {
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


// Hash code differs from cache computation.
auto hash<monsoon::tags>::operator()(const monsoon::tags& v) const noexcept
->  size_t {
  std::hash<std::string_view> key_hash;
  std::hash<monsoon::metric_value> val_hash;

  std::size_t rv = 0;
  for (const auto& e : v)
    rv = 23u * rv + 59u * key_hash(e.first) + val_hash(e.second);
  return rv;
}


} /* namespace std */
