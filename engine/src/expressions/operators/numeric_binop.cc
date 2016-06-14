#include <monsoon/expressions/operators/numeric_binop.h>

namespace monsoon {
namespace expressions {
namespace operators {


numeric_binop::~numeric_binop() noexcept {}

auto numeric_binop::evaluate(const metric_value& x, const metric_value& y)
    const
->  metric_value {
  auto x_num = x.as_number();
  auto y_num = y.as_number();
  if (!x_num.is_present() || !y_num.is_present()) return metric_value();

  return map_onto<metric_value>(x_num.get(),
      [this, &y_num](long x_val) {
        return map_onto<metric_value>(y_num.get(),
            [this, &x_val](long y_val) {
              return evaluate(x_val, y_val);
            },
            [this, &x_val](unsigned long y_val) {
              return evaluate(x_val, y_val);
            },
            [this, &x_val](double y_val) {
              return evaluate(x_val, y_val);
            });
      },
      [this, &y_num](unsigned long x_val) {
        return map_onto<metric_value>(y_num.get(),
            [this, &x_val](long y_val) {
              return evaluate(x_val, y_val);
            },
            [this, &x_val](unsigned long y_val) {
              return evaluate(x_val, y_val);
            },
            [this, &x_val](double y_val) {
              return evaluate(x_val, y_val);
            });
      },
      [this, &y_num](double x_val) {
        return map_onto<metric_value>(y_num.get(),
            [this, &x_val](long y_val) {
              return evaluate(x_val, y_val);
            },
            [this, &x_val](unsigned long y_val) {
              return evaluate(x_val, y_val);
            },
            [this, &x_val](double y_val) {
              return evaluate(x_val, y_val);
            });
      });
}


}}} /* namespace monsoon::expressions::operators */
