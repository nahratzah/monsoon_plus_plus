#include <monsoon/grammar/intf/ast.h>

namespace monsoon {
namespace grammar {
namespace ast {


value_expr::operator metric_value() const {
  return std::visit(
      [](const auto& v) { return metric_value(v); },
      *this);
}

histogram_range_expr::operator std::pair<histogram::range, std::double_t>() const {
  return std::make_pair(histogram::range(lo, hi), count);
}

histogram_expr::operator histogram() const {
  return histogram(begin(), end());
}

simple_path_lit_expr::operator metric_name() const {
  return metric_name(begin(), end());
}

simple_path_lit_expr::operator simple_group() const {
  return simple_group(begin(), end());
}


}}} /* namespace monsoon::grammar::ast */
