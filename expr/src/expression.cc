#include <monsoon/expression.h>
#include <sstream>

namespace monsoon {


expression::~expression() noexcept {}


std::string to_string(const expression& expr) {
  return (std::ostringstream() << expr).str();
}

auto operator<<(std::ostream& out, const expression& expr) -> std::ostream& {
  expr.do_ostream(out);
  return out;
}


} /* namespace monsoon */
