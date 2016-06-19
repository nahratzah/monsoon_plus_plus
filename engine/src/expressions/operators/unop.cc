#include <monsoon/expressions/operators/unop.h>
#include <ostream>
#include <stdexcept>
#include <utility>

namespace monsoon {
namespace expressions {
namespace operators {


unop::unop(const char* symbol, std::unique_ptr<expression> x)
: symbol_(symbol),
  x_(std::move(x))
{
  if (x_ == nullptr)
    throw std::invalid_argument("nullptr child expression");
}

unop::~unop() noexcept {}

auto unop::do_ostream(std::ostream& out) const -> void {
  out << symbol_ << x_->config_string();
}

auto unop::evaluate(const context& ctx) const -> expr_result {
  expr_result result = x_->evaluate(ctx);
  result.transform_values([this](metric_value&& v) {
                            return this->evaluate(std::move(v));
                          });
  return result;
}


}}} /* namespace monsoon::expressions::operators */
