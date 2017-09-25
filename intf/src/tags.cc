#include <monsoon/tags.h>
#include <monsoon/hash_support.h>
#include <monsoon/config_support.h>
#include <algorithm>
#include <utility>
#include <ostream>
#include <sstream>

namespace monsoon {


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

auto tags::tag_string() const -> std::string {
  return (std::ostringstream() << *this).str();
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


} /* namespace monsoon */


namespace std {


auto hash<monsoon::tags>::operator()(const monsoon::tags& v) const noexcept
->  size_t {
  return monsoon::map_to_hash(v.get_map());
}


} /* namespace std */
