#include <monsoon/expressions/operators/binop.h>
#include <ostream>
#include <stdexcept>
#include <utility>

namespace monsoon {
namespace expressions {
namespace operators {


binop::binop(const char* symbol,
             std::unique_ptr<expression> x, std::unique_ptr<expression> y,
             std::unique_ptr<match_clause> matcher)
: symbol_(symbol),
  x_(std::move(x)),
  y_(std::move(y)),
  matcher_(std::move(matcher))
{
  if (x_ == nullptr || y_ == nullptr || matcher_ == nullptr)
    throw std::invalid_argument("nullptr child expression");
}

binop::~binop() noexcept {}

auto binop::do_ostream(std::ostream& out) const -> void {
  out << x_->config_string() << ' ' << symbol_ << ' ';
  if (!matcher_->empty_config_string)
    out << *matcher_ << ' ';
  out << y_->config_string();
}

auto binop::evaluate(const context& ctx) const -> expr_result {
  return matcher_->apply(x_->evaluate(ctx), y_->evaluate(ctx),
                         [this](metric_value x, metric_value y) {
                           return this->evaluate(x, y);
                         });
}


}}} /* namespace monsoon::expressions::operators */
