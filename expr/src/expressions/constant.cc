#include <monsoon/expressions/constant.h>
#include <ostream>

namespace monsoon {
namespace expressions {


class monsoon_expr_local_ constant_expr
: public expression
{
 public:
  constant_expr(metric_value&& v) : v_(std::move(v)) {}
  ~constant_expr() noexcept override;

  void operator()(acceptor<emit_type>&, const metric_source&,
      const time_range&, time_point::duration) const override;

 private:
  void do_ostream(std::ostream&) const override;

  metric_value v_;
};


auto constant(metric_value v) -> expression_ptr {
  return expression::make_ptr<constant_expr>(std::move(v));
}


constant_expr::~constant_expr() noexcept {}

void constant_expr::operator()(acceptor<emit_type>& accept_fn,
    const metric_source& source, const time_range& tr,
    time_point::duration slack) const {
  source.emit_time(
      [this, &accept_fn](time_point tp) {
        accept_fn.accept(
            tp,
            std::vector<std::tuple<emit_type>>(
                1u,
                std::make_tuple(emit_type(v_))));
      },
      std::move(tr), std::move(slack));
}

void constant_expr::do_ostream(std::ostream& out) const {
  out << v_;
}


}} /* namespace monsoon::expressions */
