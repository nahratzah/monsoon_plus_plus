#include <monsoon/metric_value.h>
#include <cmath>

namespace monsoon {


auto metric_value::operator==(const metric_value& other) const noexcept
->  bool {
  return value_ == other.value_;
}

auto metric_value::as_number() const noexcept
->  optional<any<long, unsigned long, double>> {
  return map(get(),
      [](const types& v) {
        return map_onto<optional<any<long, unsigned long, double>>>(v,
            [](bool b) {
              return any<long, unsigned long, double>::create<1>(b ? 1l : 0l);
            },
            [](long v) {
              return any<long, unsigned long, double>::create<0>(v);
            },
            [](unsigned long v) {
              return any<long, unsigned long, double>::create<1>(v);
            },
            [](double v) {
              return any<long, unsigned long, double>::create<2>(v);
            },
            [](const std::string& v) {
              return optional<any<long, unsigned long, double>>();
            });
      });
}


auto operator+(const metric_value& x, const metric_value& y) noexcept
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

auto operator-(const metric_value& x, const metric_value& y) noexcept
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

auto operator*(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  const auto x_num = x.as_number();
  const auto y_num = y.as_number();
  if (!x_num.is_present() || !y_num.is_present()) return metric_value();

  return map_onto<metric_value>(x_num.get(),
      [&y_num](long x_val) {
        return map_onto<metric_value>(y_num.get(),
            [&x_val](long y_val) {
              return metric_value(x_val * y_val);
            },
            [&x_val](unsigned long y_val) {
              return metric_value(x_val * y_val);
            },
            [&x_val](double y_val) {
              return metric_value(x_val * y_val);
            });
      },
      [&y_num](unsigned long x_val) {
        return map_onto<metric_value>(y_num.get(),
            [&x_val](long y_val) {
              return metric_value(x_val * y_val);
            },
            [&x_val](unsigned long y_val) {
              return metric_value(x_val * y_val);
            },
            [&x_val](double y_val) {
              return metric_value(x_val * y_val);
            });
      },
      [&y_num](double x_val) {
        return map_onto<metric_value>(y_num.get(),
            [&x_val](long y_val) {
              return metric_value(x_val * y_val);
            },
            [&x_val](unsigned long y_val) {
              return metric_value(x_val * y_val);
            },
            [&x_val](double y_val) {
              return metric_value(x_val * y_val);
            });
      });
}

auto operator/(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  const auto x_num = x.as_number();
  const auto y_num = y.as_number();
  if (!x_num.is_present() || !y_num.is_present()) return metric_value();

  return map_onto<metric_value>(x_num.get(),
      [&y_num](long x_val) {
        return map_onto<metric_value>(y_num.get(),
            [&x_val](long y_val) {
              if (y_val == 0) return metric_value();
              return metric_value(x_val / y_val);
            },
            [&x_val](unsigned long y_val) {
              if (y_val == 0) return metric_value();
              return metric_value(x_val / y_val);
            },
            [&x_val](double y_val) {
              if (y_val == 0.0l || y_val == -0.0l) return metric_value();
              return metric_value(x_val / y_val);
            });
      },
      [&y_num](unsigned long x_val) {
        return map_onto<metric_value>(y_num.get(),
            [&x_val](long y_val) {
              if (y_val == 0) return metric_value();
              return metric_value(x_val / y_val);
            },
            [&x_val](unsigned long y_val) {
              if (y_val == 0) return metric_value();
              return metric_value(x_val / y_val);
            },
            [&x_val](double y_val) {
              if (y_val == 0.0l || y_val == -0.0l) return metric_value();
              return metric_value(x_val / y_val);
            });
      },
      [&y_num](double x_val) {
        return map_onto<metric_value>(y_num.get(),
            [&x_val](long y_val) {
              if (y_val == 0) return metric_value();
              return metric_value(x_val / y_val);
            },
            [&x_val](unsigned long y_val) {
              if (y_val == 0) return metric_value();
              return metric_value(x_val / y_val);
            },
            [&x_val](double y_val) {
              if (y_val == 0.0l || y_val == -0.0l) return metric_value();
              return metric_value(x_val / y_val);
            });
      });
}

auto operator%(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  const auto x_num = x.as_number();
  const auto y_num = y.as_number();
  if (!x_num.is_present() || !y_num.is_present()) return metric_value();

  return map_onto<metric_value>(x_num.get(),
      [&y_num](long x_val) {
        return map_onto<metric_value>(y_num.get(),
            [&x_val](long y_val) {
              if (y_val == 0) return metric_value();
              return metric_value(x_val % y_val);
            },
            [&x_val](unsigned long y_val) {
              if (y_val == 0) return metric_value();
              return metric_value(x_val % y_val);
            },
            [&x_val](double y_val) {
              if (y_val == 0.0l || y_val == -0.0l) return metric_value();
              return metric_value(std::remainder(x_val, y_val));
            });
      },
      [&y_num](unsigned long x_val) {
        return map_onto<metric_value>(y_num.get(),
            [&x_val](long y_val) {
              if (y_val == 0) return metric_value();
              return metric_value(x_val % y_val);
            },
            [&x_val](unsigned long y_val) {
              if (y_val == 0) return metric_value();
              return metric_value(x_val % y_val);
            },
            [&x_val](double y_val) {
              if (y_val == 0.0l || y_val == -0.0l) return metric_value();
              return metric_value(std::remainder(x_val, y_val));
            });
      },
      [&y_num](double x_val) {
        return map_onto<metric_value>(y_num.get(),
            [&x_val](long y_val) {
              if (y_val == 0) return metric_value();
              return metric_value(std::remainder(x_val, y_val));
            },
            [&x_val](unsigned long y_val) {
              if (y_val == 0) return metric_value();
              return metric_value(std::remainder(x_val, y_val));
            },
            [&x_val](double y_val) {
              if (y_val == 0.0l || y_val == -0.0l) return metric_value();
              return metric_value(std::remainder(x_val, y_val));
            });
      });
}


} /* namespace monsoon */


namespace std {


auto std::hash<monsoon::metric_value>::operator()(
    const monsoon::metric_value& v) const noexcept
->  size_t {
  if (!v.get().is_present()) return 0u;

  return monsoon::map_onto<size_t>(*v.get(),
                                   std::hash<bool>(),
                                   std::hash<long>(),
                                   std::hash<unsigned long>(),
                                   std::hash<double>(),
                                   std::hash<std::string>());
}


} /* namespace std */
