#include <monsoon/tags.h>
#include <monsoon/hash_support.h>

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
  return monsoon::map_to_hash(v.get_map());
}


} /* namespace std */
