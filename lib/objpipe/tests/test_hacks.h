#include <optional>
#include <ostream>
#include <vector>

namespace std {
  template<typename T>
  std::ostream& operator<<(std::ostream& out, const std::optional<T>& opt) {
    if (opt.has_value())
      return out << "optional[" << *opt << "]";
    else
      return out << "empty.optional";
  }

  template<typename T>
  std::ostream& operator<<(std::ostream& out, const std::vector<T>& v) {
    bool first = true;
    for (const auto& e : v)
      out << (std::exchange(first, false) ? "[" : ", ") << e;
    return out << "]";
  }
}
