#include <optional>
#include <ostream>

namespace std {
  template<typename T>
  std::ostream& operator<<(std::ostream& out, const std::optional<T>& opt) {
    if (opt.has_value())
      return out << "optional[" << *opt << "]";
    else
      return out << "empty.optional";
  }
}
