#ifndef _PRINT_H
#define _PRINT_H

#include <boost/io/ios_state.hpp>
#include <ostream>
#include <iomanip>

namespace std {

// HACK: print byte vector.
template<typename Alloc>
auto operator<<(std::ostream& o, const std::vector<std::uint8_t, Alloc>& v) -> std::ostream& {
  auto ifs = boost::io::ios_flags_saver(o);

  o << std::hex << std::setfill('0');
  o << "[";
  for (const auto value : v) o << " " << std::setw(2) << std::uint32_t(value);
  if (!v.empty()) o << " ";
  o << "]";

  return o;
}

}

#endif /* _PRINT_H */
