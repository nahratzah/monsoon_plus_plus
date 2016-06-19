#include <monsoon/expression.h>
#include <ostream>
#include <sstream>

namespace monsoon {


expression::~expression() noexcept {}

auto expression::config_string() const -> std::string {
  return (std::ostringstream() << *this).str();
}


auto operator<<(std::ostream& out, const expression& expr) -> std::ostream& {
  expr.do_ostream(out);
  return out;
}


} /* namespace monsoon */
