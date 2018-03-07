#ifndef HACKS_H
#define HACKS_H

#include <vector>
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


}

#endif /* HACKS_H */
