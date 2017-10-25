#include <monsoon/expressions/constant.h>
#include <ostream>
#include <functional>

namespace monsoon {
namespace expressions {


class monsoon_expr_local_ constant_expr
: public expression
{
 public:
  constant_expr(metric_value&& v) : v_(std::move(v)) {}
  ~constant_expr() noexcept override;

  auto operator()(const metric_source&,
      const time_range&, time_point::duration) const
      -> objpipe::reader<emit_type> override;

 private:
  void do_ostream(std::ostream&) const override;

  static auto transform_time_(time_point tp, metric_value v) -> emit_type;

  metric_value v_;
};


auto constant(metric_value v) -> expression_ptr {
  return expression::make_ptr<constant_expr>(std::move(v));
}


constant_expr::~constant_expr() noexcept {}

auto constant_expr::operator()(
    const metric_source& source, const time_range& tr,
    time_point::duration slack) const -> objpipe::reader<emit_type> {
  using namespace std::placeholders;

  return source.emit_time(tr, slack)
      .transform(std::bind(&constant_expr::transform_time_, _1, v_));
}

void constant_expr::do_ostream(std::ostream& out) const {
  out << v_;
}

auto constant_expr::transform_time_(time_point tp, metric_value v)
-> emit_type {
  return emit_type(tp,
      factual_emit_type(v));
}


}} /* namespace monsoon::expressions */
