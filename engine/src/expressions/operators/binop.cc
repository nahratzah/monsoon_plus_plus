#include <monsoon/expressions/operators/binop.h>
#include <stdexcept>
#include <utility>

namespace monsoon {
namespace expressions {
namespace operators {


binop::binop(const char* symbol,
             std::unique_ptr<expression> x, std::unique_ptr<expression> y)
: symbol_(symbol),
  x_(std::move(x)),
  y_(std::move(y))
{
  if (x_ == nullptr || y_ == nullptr)
    throw std::invalid_argument("nullptr child expression");
}

binop::~binop() noexcept {}

auto binop::config_string() const -> std::string {
  return x_->config_string() + symbol_ + y_->config_string();
}

auto binop::evaluate(const context& ctx) const
->  std::unordered_map<tags, metric_value> {
  std::unordered_map<tags, metric_value> result;
  const auto x_result = x_->evaluate(ctx);
  const auto y_result = y_->evaluate(ctx);

  for (const auto& y_pair : y_result) {
    const tags& key = y_pair.first;
    auto x_pair = x_result.find(key);
    if (x_pair == x_result.end()) continue;

    result.emplace(key, evaluate(x_pair->second, y_pair.second));
  }
  return result;
}


}}} /* namespace monsoon::expressions::operators */
