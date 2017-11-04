#include <monsoon/time_range.h>
#include <ostream>
#include <sstream>

namespace monsoon {


std::string to_string(const time_range& tr) {
  return (std::ostringstream() << tr).str();
}

std::ostream& operator<<(std::ostream& out, const time_range& tr) {
  bool need_space = false;
  out << "time_range(";
  if (tr.begin().has_value()) {
    if (std::exchange(need_space, true)) out << " ";
    out << "from " << *tr.begin();
  }
  if (tr.end().has_value()) {
    if (std::exchange(need_space, true)) out << " ";
    out << "until " << *tr.end();
  }
  if (tr.interval().has_value()) {
    if (std::exchange(need_space, true)) out << " ";
    out << "with interval " << *tr.interval();
  }
  out << ")";
  return out;
}


} /* namespace monsoon */
