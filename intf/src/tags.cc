#include <monsoon/tags.h>

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


} /* namespace monsoon */


namespace std {


auto hash<monsoon::tags>::operator()(const monsoon::tags& v) const noexcept
->  size_t {
  auto m_hash = std::hash<monsoon::tags::map_type>();
  return m_hash(tags.get_map());
}


} /* namespace std */
