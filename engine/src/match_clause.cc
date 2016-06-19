#include <monsoon/match_clause.h>
#include <sstream>

namespace monsoon {


match_clause::~match_clause() noexcept {}

auto match_clause::config_string() const -> std::string {
  return (std::ostringstream() << *this).str();
}


auto operator<<(std::ostream& out, const match_clause& mc) -> std::ostream& {
  if (!mc.empty_config_string) mc.do_ostream(out);
  return out;
}


} /* namespace monsoon */
