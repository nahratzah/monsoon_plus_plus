#ifndef MONSOON_METRIC_VALUE_INL_H
#define MONSOON_METRIC_VALUE_INL_H

#include <utility>

namespace monsoon {


inline metric_value::metric_value(metric_value&& other) noexcept
: value_(std::move(other.value_))
{}

inline auto metric_value::operator=(metric_value&& other) noexcept
->  metric_value& {
  value_ = std::move(other.value_);
  return *this;
}

inline metric_value::metric_value(bool v) noexcept
: value_(types::create<0>(v))
{}

inline metric_value::metric_value(long v) noexcept
: value_(types::create<1>(v))
{}

inline metric_value::metric_value(unsigned long v) noexcept
: value_(types::create<2>(v))
{}

inline metric_value::metric_value(double v) noexcept
: value_(types::create<3>(v))
{}

inline metric_value::metric_value(std::string v) noexcept
: value_(types::create<4>(std::move(v)))
{}

inline auto metric_value::operator!=(const metric_value& other) const noexcept
->  bool {
  return !(*this == other);
}

inline auto metric_value::get() const noexcept -> const optional<types>& {
  return value_;
}


inline auto operator+(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  const auto x_num = x.as_number();
  const auto y_num = y.as_number();
  if (!x_num.is_present() || !y_num.is_present()) return metric_value();

  return map_onto<metric_value>(x_num.get(),
      [&y_num](long x_val) {
        return map_onto<metric_value>(y_num.get(),
            [&x_val](long y_val) {
              return metric_value(x_val + y_val);
            },
            [&x_val](unsigned long y_val) {
              return metric_value(x_val + y_val);
            },
            [&x_val](double y_val) {
              return metric_value(x_val + y_val);
            });
      },
      [&y_num](unsigned long x_val) {
        return map_onto<metric_value>(y_num.get(),
            [&x_val](long y_val) {
              return metric_value(x_val + y_val);
            },
            [&x_val](unsigned long y_val) {
              return metric_value(x_val + y_val);
            },
            [&x_val](double y_val) {
              return metric_value(x_val + y_val);
            });
      },
      [&y_num](double x_val) {
        return map_onto<metric_value>(y_num.get(),
            [&x_val](long y_val) {
              return metric_value(x_val + y_val);
            },
            [&x_val](unsigned long y_val) {
              return metric_value(x_val + y_val);
            },
            [&x_val](double y_val) {
              return metric_value(x_val + y_val);
            });
      });
}

inline auto operator-(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  const auto x_num = x.as_number();
  const auto y_num = y.as_number();
  if (!x_num.is_present() || !y_num.is_present()) return metric_value();

  return map_onto<metric_value>(x_num.get(),
      [&y_num](long x_val) {
        return map_onto<metric_value>(y_num.get(),
            [&x_val](long y_val) {
              return metric_value(x_val - y_val);
            },
            [&x_val](unsigned long y_val) {
              return metric_value(x_val - y_val);
            },
            [&x_val](double y_val) {
              return metric_value(x_val - y_val);
            });
      },
      [&y_num](unsigned long x_val) {
        return map_onto<metric_value>(y_num.get(),
            [&x_val](long y_val) {
              return metric_value(x_val - y_val);
            },
            [&x_val](unsigned long y_val) {
              return metric_value(x_val - y_val);
            },
            [&x_val](double y_val) {
              return metric_value(x_val - y_val);
            });
      },
      [&y_num](double x_val) {
        return map_onto<metric_value>(y_num.get(),
            [&x_val](long y_val) {
              return metric_value(x_val - y_val);
            },
            [&x_val](unsigned long y_val) {
              return metric_value(x_val - y_val);
            },
            [&x_val](double y_val) {
              return metric_value(x_val - y_val);
            });
      });
}


} /* namespace monsoon */

#endif /* MONSOON_METRIC_VALUE_INL_H */
