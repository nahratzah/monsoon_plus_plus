#include <monsoon/metric_value.h>
#include <monsoon/config_support.h>
#include <cmath>

namespace monsoon {


auto metric_value::operator==(const metric_value& other) const noexcept
->  bool {
  return value_ == other.value_;
}

auto metric_value::as_bool() const noexcept -> optional<bool> {
  return map(get(),
      [](const types& v) {
        return map_onto<optional<bool>>(v,
            [](bool b) {
              return b;
            },
            [](long v) {
              return v != 0;
            },
            [](unsigned long v) {
              return v != 0;
            },
            [](double v) {
              return v != 0.0l && v != -0.0l;
            },
            [](const std::string& s) {
              return optional<bool>();
            },
            [](const histogram& h) {
              return !h.empty();
            });
      });
}

auto metric_value::as_number() const noexcept
->  optional<any<long, unsigned long, double>> {
  using result_types = any<long, unsigned long, double>;

  return map(get(),
      [](const types& v) {
        return map_onto<optional<result_types>>(v,
            [](bool b) {
              return result_types::create<1>(b ? 1l : 0l);
            },
            [](long v) {
              return result_types::create<0>(v);
            },
            [](unsigned long v) {
              return result_types::create<1>(v);
            },
            [](double v) {
              return result_types::create<2>(v);
            },
            [](const std::string& v) {
              return optional<result_types>();
            },
            [](const histogram& h) {
              return optional<result_types>();
            });
      });
}

auto metric_value::as_number_or_histogram() const noexcept
->  optional<any<long, unsigned long, double, histogram>> {
  using result_types = any<long, unsigned long, double, histogram>;

  return map(get(),
      [](const types& v) {
        return map_onto<optional<result_types>>(v,
            [](bool b) {
              return result_types::create<1>(b ? 1l : 0l);
            },
            [](long v) {
              return result_types::create<0>(v);
            },
            [](unsigned long v) {
              return result_types::create<1>(v);
            },
            [](double v) {
              return result_types::create<2>(v);
            },
            [](const std::string& v) {
              return optional<result_types>();
            },
            [](const histogram& h) {
              return result_types::create<3>(h);
            });
      });
}

auto metric_value::as_string() const -> optional<std::string> {
  return map(get(),
      [](const types& v) {
        return map_onto<optional<std::string>>(v,
            [](bool b) {
              return std::string(b ? "true" : "false");
            },
            [](long v) {
              return std::to_string(v);
            },
            [](unsigned long v) {
              return std::to_string(v);
            },
            [](double v) {
              return std::to_string(v);
            },
            [](const std::string& v) {
              return v;
            },
            [](const histogram& h) {
              return optional<std::string>();
            });
      });
}


auto operator!(const metric_value& x) noexcept -> metric_value {
  return map(x.as_bool(), [](bool b) { return metric_value(!b); });
}

auto operator&&(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  return map(x.as_bool(),
      [&y](bool x_bool) {
        return map(y.as_bool(),
            [&x_bool](bool y_bool) {
              return metric_value(x_bool && y_bool);
            });
      });
}

auto operator||(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  return map(x.as_bool(),
      [&y](bool x_bool) {
        return map(y.as_bool(),
            [&x_bool](bool y_bool) {
              return metric_value(x_bool || y_bool);
            });
      });
}

auto operator-(const metric_value& x) noexcept -> metric_value {
  auto x_num = x.as_number_or_histogram();
  if (!x_num.is_present()) return metric_value();

  return map_onto<metric_value>(x_num.release(),
      [](long x_val) {
        return metric_value(-x_val);
      },
      [](unsigned long x_val) {
        long rv = x_val;
        return metric_value(-rv);
      },
      [](double x_val) {
        return metric_value(-x_val);
      },
      [](histogram&& x_val) {
        return metric_value(-std::move(x_val));
      });
}

auto operator+(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  auto x_num = x.as_number_or_histogram();
  auto y_num = y.as_number_or_histogram();
  if (!x_num.is_present() || !y_num.is_present()) return metric_value();

  return map_onto<metric_value>(x_num.release(),
      [&y_num](long x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](long y_val) {
              return metric_value(x_val + y_val);
            },
            [&x_val](unsigned long y_val) {
              return metric_value(x_val + y_val);
            },
            [&x_val](double y_val) {
              return metric_value(x_val + y_val);
            },
            [&x_val](histogram&& y_val) {
              return metric_value();
            });
      },
      [&y_num](unsigned long x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](long y_val) {
              return metric_value(x_val + y_val);
            },
            [&x_val](unsigned long y_val) {
              return metric_value(x_val + y_val);
            },
            [&x_val](double y_val) {
              return metric_value(x_val + y_val);
            },
            [&x_val](histogram&& y_val) {
              return metric_value();
            });
      },
      [&y_num](double x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](long y_val) {
              return metric_value(x_val + y_val);
            },
            [&x_val](unsigned long y_val) {
              return metric_value(x_val + y_val);
            },
            [&x_val](double y_val) {
              return metric_value(x_val + y_val);
            },
            [&x_val](histogram&& y_val) {
              return metric_value();
            });
      },
      [&y_num](histogram&& x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](long y_val) {
              return metric_value();
            },
            [&x_val](unsigned long y_val) {
              return metric_value();
            },
            [&x_val](double y_val) {
              return metric_value();
            },
            [&x_val](histogram&& y_val) {
              return metric_value(std::move(x_val) + std::move(y_val));
            });
      });
}

auto operator-(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  auto x_num = x.as_number_or_histogram();
  auto y_num = y.as_number_or_histogram();
  if (!x_num.is_present() || !y_num.is_present()) return metric_value();

  return map_onto<metric_value>(x_num.release(),
      [&y_num](long x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](long y_val) {
              return metric_value(x_val - y_val);
            },
            [&x_val](unsigned long y_val) {
              return metric_value(x_val - y_val);
            },
            [&x_val](double y_val) {
              return metric_value(x_val - y_val);
            },
            [&x_val](histogram&& y_val) {
              return metric_value();
            });
      },
      [&y_num](unsigned long x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](long y_val) {
              return metric_value(x_val - y_val);
            },
            [&x_val](unsigned long y_val) {
              if (y_val > x_val) {
                long rv = y_val - x_val;
                return metric_value(-rv);
              }
              return metric_value(x_val - y_val);
            },
            [&x_val](double y_val) {
              return metric_value(x_val - y_val);
            },
            [&x_val](histogram&& y_val) {
              return metric_value();
            });
      },
      [&y_num](double x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](long y_val) {
              return metric_value(x_val - y_val);
            },
            [&x_val](unsigned long y_val) {
              return metric_value(x_val - y_val);
            },
            [&x_val](double y_val) {
              return metric_value(x_val - y_val);
            },
            [&x_val](histogram&& y_val) {
              return metric_value();
            });
      },
      [&y_num](histogram&& x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](long y_val) {
              return metric_value();
            },
            [&x_val](unsigned long y_val) {
              return metric_value();
            },
            [&x_val](double y_val) {
              return metric_value();
            },
            [&x_val](histogram&& y_val) {
              return metric_value(std::move(x_val) - std::move(y_val));
            });
      });
}

auto operator*(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  auto x_num = x.as_number_or_histogram();
  auto y_num = y.as_number_or_histogram();
  if (!x_num.is_present() || !y_num.is_present()) return metric_value();

  return map_onto<metric_value>(x_num.release(),
      [&y_num](long x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](long y_val) {
              return metric_value(x_val * y_val);
            },
            [&x_val](unsigned long y_val) {
              return metric_value(x_val * y_val);
            },
            [&x_val](double y_val) {
              return metric_value(x_val * y_val);
            },
            [&x_val](histogram&& y_val) {
              return metric_value(x_val * std::move(y_val));
            });
      },
      [&y_num](unsigned long x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](long y_val) {
              return metric_value(x_val * y_val);
            },
            [&x_val](unsigned long y_val) {
              return metric_value(x_val * y_val);
            },
            [&x_val](double y_val) {
              return metric_value(x_val * y_val);
            },
            [&x_val](histogram&& y_val) {
              return metric_value(x_val * std::move(y_val));
            });
      },
      [&y_num](double x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](long y_val) {
              return metric_value(x_val * y_val);
            },
            [&x_val](unsigned long y_val) {
              return metric_value(x_val * y_val);
            },
            [&x_val](double y_val) {
              return metric_value(x_val * y_val);
            },
            [&x_val](histogram&& y_val) {
              return metric_value(x_val * std::move(y_val));
            });
      },
      [&y_num](histogram&& x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](long y_val) {
              return metric_value(std::move(x_val) * y_val);
            },
            [&x_val](unsigned long y_val) {
              return metric_value(std::move(x_val) * y_val);
            },
            [&x_val](double y_val) {
              return metric_value(std::move(x_val) * y_val);
            },
            [&x_val](histogram&& y_val) {
              return metric_value();
            });
      });
}

auto operator/(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  auto x_num = x.as_number_or_histogram();
  auto y_num = y.as_number_or_histogram();
  if (!x_num.is_present() || !y_num.is_present()) return metric_value();

  return map_onto<metric_value>(x_num.release(),
      [&y_num](long x_val) {
        return map_onto<metric_value>(y_num.release(),
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
            },
            [&x_val](histogram&& y_val) {
              return metric_value();
            });
      },
      [&y_num](unsigned long x_val) {
        return map_onto<metric_value>(y_num.release(),
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
            },
            [&x_val](histogram&& y_val) {
              return metric_value();
            });
      },
      [&y_num](double x_val) {
        return map_onto<metric_value>(y_num.release(),
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
            },
            [&x_val](histogram&& y_val) {
              return metric_value();
            });
      },
      [&y_num](histogram&& x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](long y_val) {
              if (y_val == 0) return metric_value();
              return metric_value(std::move(x_val) / y_val);
            },
            [&x_val](unsigned long y_val) {
              if (y_val == 0) return metric_value();
              return metric_value(std::move(x_val) / y_val);
            },
            [&x_val](double y_val) {
              if (y_val == 0.0l || y_val == -0.0l) return metric_value();
              return metric_value(std::move(x_val) / y_val);
            },
            [&x_val](histogram&& y_val) {
              return metric_value();
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
                                   std::hash<std::string>(),
                                   std::hash<monsoon::histogram>());
}

auto to_string(const monsoon::metric_value& v) -> std::string {
  if (!v.get().is_present()) return "(none)";

  return monsoon::map_onto<std::string>(*v.get(),
      [](bool b) {
        return (b ? "true" : "false");
      },
      [](long v) {
        return to_string(v);
      },
      [](unsigned long v) {
        return to_string(v);
      },
      [](double v) {
        return to_string(v);
      },
      [](const string& v) {
        return monsoon::quoted_string(v);
      },
      [](const monsoon::histogram& v) {
        return to_string(v);
      });
}


} /* namespace std */
