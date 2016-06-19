#include <monsoon/tags.h>
#include <monsoon/hash_support.h>
#include <monsoon/config_support.h>
#include <algorithm>
#include <utility>
#include <ostream>
#include <sstream>

namespace monsoon {


auto tags::operator[](const std::string& key) const noexcept
->  optional<const metric_value&> {
  auto pos = map_.find(key);
  return ( pos == map_.end()
         ? optional<const metric_value&>()
         : pos->second);
}

auto tags::operator==(const tags& other) const noexcept -> bool {
  return map_ == other.map_;
}

auto tags::tag_string() const -> std::string {
  return (std::ostringstream() << *this).str();
}


auto operator<<(std::ostream& out, const tags& tv) -> std::ostream& {
  if (tv.empty()) return out << "{}";

  // We emit tags in sorted order.
  std::vector<std::tuple<std::string, metric_value>> t(tv.get_map().begin(),
                                                       tv.get_map().end());
  std::sort(t.begin(), t.end(),
            [](const std::tuple<std::string, metric_value>& x,
               const std::tuple<std::string, metric_value>& y) {
              return std::get<0>(x) < std::get<0>(y);
            });

  bool first = true;
  out << "{";
  for (const auto& s : t) {
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
