#include <monsoon/expressions/constant.h>
#include <ostream>
#include <functional>

namespace monsoon {
namespace expressions {


class monsoon_expr_local_ constant_expr
: public expression
{
 public:
  constant_expr(metric_value&& v)
  : expression(precedence_value),
    v_(std::move(v))
  {}

  ~constant_expr() noexcept override;

  auto operator()(const metric_source&,
      const time_range&, time_point::duration,
      const std::shared_ptr<const match_clause>&) const
      -> std::variant<scalar_objpipe, vector_objpipe> override;

  bool is_scalar() const noexcept override;
  bool is_vector() const noexcept override;

 private:
  void do_ostream(std::ostream&) const override;

  static auto transform_time_(time_point, metric_value) -> scalar_emit_type;

  metric_value v_;
};


auto constant(metric_value v) -> expression_ptr {
  return expression::make_ptr<constant_expr>(std::move(v));
}


constant_expr::~constant_expr() noexcept {}

auto constant_expr::operator()(
    const metric_source& source, const time_range& tr,
    time_point::duration slack,
    const std::shared_ptr<const match_clause>&) const
-> std::variant<scalar_objpipe, vector_objpipe> {
  using namespace std::placeholders;

  return source.emit_time(tr, slack)
      .transform(std::bind(&constant_expr::transform_time_, _1, v_));
}

bool constant_expr::is_scalar() const noexcept {
  return true;
}

bool constant_expr::is_vector() const noexcept {
  return false;
}

void constant_expr::do_ostream(std::ostream& out) const {
  out << v_;
}

auto constant_expr::transform_time_(time_point tp, metric_value v)
-> scalar_emit_type {
  return { tp, std::in_place_index<1>, v };
}


}} /* namespace monsoon::expressions */
