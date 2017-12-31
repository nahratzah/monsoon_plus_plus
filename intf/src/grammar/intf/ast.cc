#include <monsoon/grammar/intf/ast.h>

namespace monsoon {
namespace grammar {
namespace ast {


value_expr::operator metric_value() const {
  using std::bind;
  using namespace std::placeholders;

  return this->apply_visitor(
      bind<metric_value>(
          [](const auto& v) { return metric_value(v); },
          _1));
}


}}} /* namespace monsoon::grammar::ast */
