#ifndef HACKS_H
#define HACKS_H

#include <vector>
#include <map>
#include <ostream>

namespace std {


template<typename T, typename A>
auto operator<<(std::ostream& out, const std::vector<T, A>& v)
-> std::ostream& {
  bool first = true;
  for (const auto& e : v) {
    if (std::exchange(first, false))
      out << "[ ";
    else
      out << ", ";
    out << e;
  }
  return out << (first ? "[]" : " ]");
}

template<typename K, typename V, typename Less, typename Alloc>
auto operator<<(std::ostream& out, const std::map<K, V, Less, Alloc>& m)
-> std::ostream& {
  bool first = true;
  for (const auto& e : m) {
    out << (std::exchange(first, false) ? "{ " : ", ")
        << e.first << ": " << e.second;
  }
  return out << (std::exchange(first, false) ? "{}" : " }");
}


} /* namespace std */

#endif /* HACKS_H */
