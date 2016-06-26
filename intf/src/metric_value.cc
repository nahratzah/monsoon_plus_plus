#include <monsoon/metric_value.h>
#include <monsoon/config_support.h>
#include <cmath>
#include <ostream>

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
            [](signed_type v) {
              return v != 0;
            },
            [](unsigned_type v) {
              return v != 0;
            },
            [](fp_type v) {
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
->  optional<any<signed_type, unsigned_type, fp_type>> {
  using result_types = any<signed_type, unsigned_type, fp_type>;

  return map(get(),
      [](const types& v) {
        return map_onto<optional<result_types>>(v,
            [](bool b) {
              return result_types::create<1>(b ? 1l : 0l);
            },
            [](signed_type v) {
              return result_types::create<0>(v);
            },
            [](unsigned_type v) {
              return result_types::create<1>(v);
            },
            [](fp_type v) {
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
->  optional<any<signed_type, unsigned_type, fp_type, histogram>> {
  using result_types = any<signed_type, unsigned_type, fp_type, histogram>;

  return map(get(),
      [](const types& v) {
        return map_onto<optional<result_types>>(v,
            [](bool b) {
              return result_types::create<1>(b ? 1l : 0l);
            },
            [](signed_type v) {
              return result_types::create<0>(v);
            },
            [](unsigned_type v) {
              return result_types::create<1>(v);
            },
            [](fp_type v) {
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
            [](signed_type v) {
              return std::to_string(v);
            },
            [](unsigned_type v) {
              return std::to_string(v);
            },
            [](fp_type v) {
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

auto metric_value::before(const metric_value& x, const metric_value& y)
    noexcept
->  bool {
  if (!x.get().is_present() || !y.get().is_present())
    return x.get().is_present() < y.get().is_present();
  if (x.get()->selector() != y.get()->selector())
    return x.get()->selector() < y.get()->selector();

  return map_onto<bool>(*x.get(),
      [&y](bool xval) {
        return xval < monsoon::get<0>(*y.get());
      },
      [&y](signed_type xval) {
        return xval < monsoon::get<1>(*y.get());
      },
      [&y](unsigned_type xval) {
        return xval < monsoon::get<2>(*y.get());
      },
      [&y](fp_type xval) {
        return xval < monsoon::get<3>(*y.get());
      },
      [&y](const std::string& xval) {
        return xval < monsoon::get<4>(*y.get());
      },
      [&y](const histogram& xval) {
        return histogram::before(xval, monsoon::get<5>(*y.get()));
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
  using unsigned_type = metric_value::unsigned_type;
  using signed_type = metric_value::signed_type;
  using fp_type = metric_value::fp_type;

  auto x_num = x.as_number_or_histogram();
  if (!x_num.is_present()) return metric_value();

  return map_onto<metric_value>(x_num.release(),
      [](signed_type x_val) {
        return metric_value(-x_val);
      },
      [](unsigned_type x_val) {
        signed_type rv = x_val;
        return metric_value(-rv);
      },
      [](fp_type x_val) {
        return metric_value(-x_val);
      },
      [](histogram&& x_val) {
        return metric_value(-std::move(x_val));
      });
}

auto operator+(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  using unsigned_type = metric_value::unsigned_type;
  using signed_type = metric_value::signed_type;
  using fp_type = metric_value::fp_type;

  auto x_num = x.as_number_or_histogram();
  auto y_num = y.as_number_or_histogram();
  if (!x_num.is_present() || !y_num.is_present()) return metric_value();

  return map_onto<metric_value>(x_num.release(),
      [&y_num](signed_type x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](signed_type y_val) {
              return metric_value(x_val + y_val);
            },
            [&x_val](unsigned_type y_val) {
              return metric_value(x_val + y_val);
            },
            [&x_val](fp_type y_val) {
              return metric_value(x_val + y_val);
            },
            [&x_val](histogram&& y_val) {
              return metric_value();
            });
      },
      [&y_num](unsigned_type x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](signed_type y_val) {
              return metric_value(x_val + y_val);
            },
            [&x_val](unsigned_type y_val) {
              return metric_value(x_val + y_val);
            },
            [&x_val](fp_type y_val) {
              return metric_value(x_val + y_val);
            },
            [&x_val](histogram&& y_val) {
              return metric_value();
            });
      },
      [&y_num](fp_type x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](signed_type y_val) {
              return metric_value(x_val + y_val);
            },
            [&x_val](unsigned_type y_val) {
              return metric_value(x_val + y_val);
            },
            [&x_val](fp_type y_val) {
              return metric_value(x_val + y_val);
            },
            [&x_val](histogram&& y_val) {
              return metric_value();
            });
      },
      [&y_num](histogram&& x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](signed_type y_val) {
              return metric_value();
            },
            [&x_val](unsigned_type y_val) {
              return metric_value();
            },
            [&x_val](fp_type y_val) {
              return metric_value();
            },
            [&x_val](histogram&& y_val) {
              return metric_value(std::move(x_val) + std::move(y_val));
            });
      });
}

auto operator-(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  using unsigned_type = metric_value::unsigned_type;
  using signed_type = metric_value::signed_type;
  using fp_type = metric_value::fp_type;

  auto x_num = x.as_number_or_histogram();
  auto y_num = y.as_number_or_histogram();
  if (!x_num.is_present() || !y_num.is_present()) return metric_value();

  return map_onto<metric_value>(x_num.release(),
      [&y_num](signed_type x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](signed_type y_val) {
              return metric_value(x_val - y_val);
            },
            [&x_val](unsigned_type y_val) {
              return metric_value(x_val - y_val);
            },
            [&x_val](fp_type y_val) {
              return metric_value(x_val - y_val);
            },
            [&x_val](histogram&& y_val) {
              return metric_value();
            });
      },
      [&y_num](unsigned_type x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](signed_type y_val) {
              return metric_value(x_val - y_val);
            },
            [&x_val](unsigned_type y_val) {
              if (y_val > x_val) {
                signed_type rv = y_val - x_val;
                return metric_value(-rv);
              }
              return metric_value(x_val - y_val);
            },
            [&x_val](fp_type y_val) {
              return metric_value(x_val - y_val);
            },
            [&x_val](histogram&& y_val) {
              return metric_value();
            });
      },
      [&y_num](fp_type x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](signed_type y_val) {
              return metric_value(x_val - y_val);
            },
            [&x_val](unsigned_type y_val) {
              return metric_value(x_val - y_val);
            },
            [&x_val](fp_type y_val) {
              return metric_value(x_val - y_val);
            },
            [&x_val](histogram&& y_val) {
              return metric_value();
            });
      },
      [&y_num](histogram&& x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](signed_type y_val) {
              return metric_value();
            },
            [&x_val](unsigned_type y_val) {
              return metric_value();
            },
            [&x_val](fp_type y_val) {
              return metric_value();
            },
            [&x_val](histogram&& y_val) {
              return metric_value(std::move(x_val) - std::move(y_val));
            });
      });
}

auto operator*(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  using unsigned_type = metric_value::unsigned_type;
  using signed_type = metric_value::signed_type;
  using fp_type = metric_value::fp_type;

  auto x_num = x.as_number_or_histogram();
  auto y_num = y.as_number_or_histogram();
  if (!x_num.is_present() || !y_num.is_present()) return metric_value();

  return map_onto<metric_value>(x_num.release(),
      [&y_num](signed_type x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](signed_type y_val) {
              return metric_value(x_val * y_val);
            },
            [&x_val](unsigned_type y_val) {
              return metric_value(x_val * y_val);
            },
            [&x_val](fp_type y_val) {
              return metric_value(x_val * y_val);
            },
            [&x_val](histogram&& y_val) {
              return metric_value(x_val * std::move(y_val));
            });
      },
      [&y_num](unsigned_type x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](signed_type y_val) {
              return metric_value(x_val * y_val);
            },
            [&x_val](unsigned_type y_val) {
              return metric_value(x_val * y_val);
            },
            [&x_val](fp_type y_val) {
              return metric_value(x_val * y_val);
            },
            [&x_val](histogram&& y_val) {
              return metric_value(x_val * std::move(y_val));
            });
      },
      [&y_num](fp_type x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](signed_type y_val) {
              return metric_value(x_val * y_val);
            },
            [&x_val](unsigned_type y_val) {
              return metric_value(x_val * y_val);
            },
            [&x_val](fp_type y_val) {
              return metric_value(x_val * y_val);
            },
            [&x_val](histogram&& y_val) {
              return metric_value(x_val * std::move(y_val));
            });
      },
      [&y_num](histogram&& x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](signed_type y_val) {
              return metric_value(std::move(x_val) * y_val);
            },
            [&x_val](unsigned_type y_val) {
              return metric_value(std::move(x_val) * y_val);
            },
            [&x_val](fp_type y_val) {
              return metric_value(std::move(x_val) * y_val);
            },
            [&x_val](histogram&& y_val) {
              return metric_value();
            });
      });
}

auto operator/(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  using unsigned_type = metric_value::unsigned_type;
  using signed_type = metric_value::signed_type;
  using fp_type = metric_value::fp_type;

  auto x_num = x.as_number_or_histogram();
  auto y_num = y.as_number_or_histogram();
  if (!x_num.is_present() || !y_num.is_present()) return metric_value();

  return map_onto<metric_value>(x_num.release(),
      [&y_num](signed_type x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](signed_type y_val) {
              if (y_val == 0) return metric_value();
              return metric_value(x_val / y_val);
            },
            [&x_val](unsigned_type y_val) {
              if (y_val == 0) return metric_value();
              return metric_value(x_val / y_val);
            },
            [&x_val](fp_type y_val) {
              if (y_val == 0.0l || y_val == -0.0l) return metric_value();
              return metric_value(x_val / y_val);
            },
            [&x_val](histogram&& y_val) {
              return metric_value();
            });
      },
      [&y_num](unsigned_type x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](signed_type y_val) {
              if (y_val == 0) return metric_value();
              return metric_value(x_val / y_val);
            },
            [&x_val](unsigned_type y_val) {
              if (y_val == 0) return metric_value();
              return metric_value(x_val / y_val);
            },
            [&x_val](fp_type y_val) {
              if (y_val == 0.0l || y_val == -0.0l) return metric_value();
              return metric_value(x_val / y_val);
            },
            [&x_val](histogram&& y_val) {
              return metric_value();
            });
      },
      [&y_num](fp_type x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](signed_type y_val) {
              if (y_val == 0) return metric_value();
              return metric_value(x_val / y_val);
            },
            [&x_val](unsigned_type y_val) {
              if (y_val == 0) return metric_value();
              return metric_value(x_val / y_val);
            },
            [&x_val](fp_type y_val) {
              if (y_val == 0.0l || y_val == -0.0l) return metric_value();
              return metric_value(x_val / y_val);
            },
            [&x_val](histogram&& y_val) {
              return metric_value();
            });
      },
      [&y_num](histogram&& x_val) {
        return map_onto<metric_value>(y_num.release(),
            [&x_val](signed_type y_val) {
              if (y_val == 0) return metric_value();
              return metric_value(std::move(x_val) / y_val);
            },
            [&x_val](unsigned_type y_val) {
              if (y_val == 0) return metric_value();
              return metric_value(std::move(x_val) / y_val);
            },
            [&x_val](fp_type y_val) {
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
  using unsigned_type = metric_value::unsigned_type;
  using signed_type = metric_value::signed_type;
  using fp_type = metric_value::fp_type;

  const auto x_num = x.as_number();
  const auto y_num = y.as_number();
  if (!x_num.is_present() || !y_num.is_present()) return metric_value();

  return map_onto<metric_value>(x_num.get(),
      [&y_num](signed_type x_val) {
        return map_onto<metric_value>(y_num.get(),
            [&x_val](signed_type y_val) {
              if (y_val == 0) return metric_value();
              return metric_value(x_val % y_val);
            },
            [&x_val](unsigned_type y_val) {
              if (y_val == 0) return metric_value();
              return metric_value(x_val % y_val);
            },
            [&x_val](fp_type y_val) {
              if (y_val == 0.0l || y_val == -0.0l) return metric_value();
              return metric_value(std::remainder(x_val, y_val));
            });
      },
      [&y_num](unsigned_type x_val) {
        return map_onto<metric_value>(y_num.get(),
            [&x_val](signed_type y_val) {
              if (y_val == 0) return metric_value();
              return metric_value(x_val % y_val);
            },
            [&x_val](unsigned_type y_val) {
              if (y_val == 0) return metric_value();
              return metric_value(x_val % y_val);
            },
            [&x_val](fp_type y_val) {
              if (y_val == 0.0l || y_val == -0.0l) return metric_value();
              return metric_value(std::remainder(x_val, y_val));
            });
      },
      [&y_num](fp_type x_val) {
        return map_onto<metric_value>(y_num.get(),
            [&x_val](signed_type y_val) {
              if (y_val == 0) return metric_value();
              return metric_value(std::remainder(x_val, y_val));
            },
            [&x_val](unsigned_type y_val) {
              if (y_val == 0) return metric_value();
              return metric_value(std::remainder(x_val, y_val));
            },
            [&x_val](fp_type y_val) {
              if (y_val == 0.0l || y_val == -0.0l) return metric_value();
              return metric_value(std::remainder(x_val, y_val));
            });
      });
}

auto operator<<(std::ostream& out, const metric_value& v) -> std::ostream& {
  using unsigned_type = metric_value::unsigned_type;
  using signed_type = metric_value::signed_type;
  using fp_type = metric_value::fp_type;

  if (!v.get().is_present()) return out << "(none)";

  return monsoon::map_onto<std::ostream&>(*v.get(),
      [&out](bool b) -> std::ostream& {
        return out << (b ? "true" : "false");
      },
      [&out](signed_type v) -> std::ostream& {
        return out << v;
      },
      [&out](unsigned_type v) -> std::ostream& {
        return out << v;
      },
      [&out](fp_type v) -> std::ostream& {
        return out << v;
      },
      [&out](const std::string& v) -> std::ostream& {
        return out << monsoon::quoted_string(v);
      },
      [&out](const histogram& v) -> std::ostream& {
        return out << v;
      });
}

auto equal(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  using unsigned_type = metric_value::unsigned_type;
  using signed_type = metric_value::signed_type;
  using fp_type = metric_value::fp_type;

  if (!x.get().is_present() || !y.get().is_present()) return metric_value();

  return map_onto<metric_value>(*x.get(),
      [&y](bool x_val) {
        return map_onto<metric_value>(*y.get(),
            [&x_val](bool y_val) {
              return metric_value(x_val == y_val);
            },
            [&x_val](signed_type y_val) {
              return metric_value(x_val == y_val);
            },
            [&x_val](unsigned y_val) {
              return metric_value(x_val == y_val);
            },
            [&x_val](fp_type y_val) {
              return metric_value(x_val == y_val);
            },
            [&x_val](const std::string& y_val) {
              return metric_value();
            },
            [&x_val](const histogram& y_val) {
              return metric_value();
            });
      },
      [&y](signed_type x_val) {
        return map_onto<metric_value>(*y.get(),
            [&x_val](bool y_val) {
              return metric_value(x_val == y_val);
            },
            [&x_val](signed_type y_val) {
              return metric_value(x_val == y_val);
            },
            [&x_val](unsigned y_val) {
              return metric_value(x_val == y_val);
            },
            [&x_val](fp_type y_val) {
              return metric_value(x_val == y_val);
            },
            [&x_val](const std::string& y_val) {
              return metric_value();
            },
            [&x_val](const histogram& y_val) {
              return metric_value();
            });
      },
      [&y](unsigned_type x_val) {
        return map_onto<metric_value>(*y.get(),
            [&x_val](bool y_val) {
              return metric_value(x_val == y_val);
            },
            [&x_val](signed_type y_val) {
              return metric_value(x_val == y_val);
            },
            [&x_val](unsigned y_val) {
              return metric_value(x_val == y_val);
            },
            [&x_val](fp_type y_val) {
              return metric_value(x_val == y_val);
            },
            [&x_val](const std::string& y_val) {
              return metric_value();
            },
            [&x_val](const histogram& y_val) {
              return metric_value();
            });
      },
      [&y](fp_type x_val) {
        return map_onto<metric_value>(*y.get(),
            [&x_val](bool y_val) {
              return metric_value(x_val == y_val);
            },
            [&x_val](signed_type y_val) {
              return metric_value(x_val == y_val);
            },
            [&x_val](unsigned y_val) {
              return metric_value(x_val == y_val);
            },
            [&x_val](fp_type y_val) {
              return metric_value(x_val == y_val);
            },
            [&x_val](const std::string& y_val) {
              return metric_value();
            },
            [&x_val](const histogram& y_val) {
              return metric_value();
            });
      },
      [&y](const std::string& x_val) {
        return map_onto<metric_value>(*y.get(),
            [&x_val](bool y_val) {
              return metric_value();
            },
            [&x_val](signed_type y_val) {
              return metric_value();
            },
            [&x_val](unsigned y_val) {
              return metric_value();
            },
            [&x_val](fp_type y_val) {
              return metric_value();
            },
            [&x_val](const std::string& y_val) {
              return metric_value(x_val == y_val);
            },
            [&x_val](const histogram& y_val) {
              return metric_value();
            });
      },
      [&y](const histogram& x_val) {
        return map_onto<metric_value>(*y.get(),
            [&x_val](bool y_val) {
              return metric_value();
            },
            [&x_val](signed_type y_val) {
              return metric_value();
            },
            [&x_val](unsigned y_val) {
              return metric_value();
            },
            [&x_val](fp_type y_val) {
              return metric_value();
            },
            [&x_val](const std::string& y_val) {
              return metric_value();
            },
            [&x_val](const histogram& y_val) {
              return metric_value(x_val == y_val);
            });
      });
}

auto unequal(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  return !equal(x, y);
}

auto less(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  using unsigned_type = metric_value::unsigned_type;
  using signed_type = metric_value::signed_type;
  using fp_type = metric_value::fp_type;

  const auto x_num = x.as_number();
  const auto y_num = y.as_number();
  if (!x_num.is_present() || !y_num.is_present()) return metric_value();

  return map_onto<metric_value>(x_num.get(),
      [&y_num](signed_type x_val) {
        assert(x_val < 0);
        return map_onto<metric_value>(y_num.get(),
            [&x_val](signed_type y_val) {
              return metric_value(x_val < y_val);
            },
            [&x_val](unsigned_type y_val) {
              return metric_value(false);
            },
            [&x_val](fp_type y_val) {
              return metric_value(x_val < y_val);
            });
      },
      [&y_num](unsigned_type x_val) {
        return map_onto<metric_value>(y_num.get(),
            [&x_val](signed_type y_val) {
              assert(y_val < 0);
              return metric_value(false);
            },
            [&x_val](unsigned_type y_val) {
              return metric_value(x_val < y_val);
            },
            [&x_val](fp_type y_val) {
              return metric_value(x_val < y_val);
            });
      },
      [&y_num](fp_type x_val) {
        return map_onto<metric_value>(y_num.get(),
            [&x_val](signed_type y_val) {
              return metric_value(x_val < y_val);
            },
            [&x_val](unsigned_type y_val) {
              return metric_value(x_val < y_val);
            },
            [&x_val](fp_type y_val) {
              return metric_value(x_val < y_val);
            });
      });
}

auto greater(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  return less(y, x);
}

auto less_equal(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  return !less(y, x);
}

auto greater_equal(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  return !less(x, y);
}


} /* namespace monsoon */


namespace std {


auto std::hash<monsoon::metric_value>::operator()(
    const monsoon::metric_value& v) const noexcept
->  size_t {
  using unsigned_type = monsoon::metric_value::unsigned_type;
  using signed_type = monsoon::metric_value::signed_type;
  using fp_type = monsoon::metric_value::fp_type;

  if (!v.get().is_present()) return 0u;

  return monsoon::map_onto<size_t>(*v.get(),
                                   std::hash<bool>(),
                                   std::hash<signed_type>(),
                                   std::hash<unsigned_type>(),
                                   std::hash<fp_type>(),
                                   std::hash<std::string>(),
                                   std::hash<monsoon::histogram>());
}

auto to_string(const monsoon::metric_value& v) -> std::string {
  using unsigned_type = monsoon::metric_value::unsigned_type;
  using signed_type = monsoon::metric_value::signed_type;
  using fp_type = monsoon::metric_value::fp_type;

  if (!v.get().is_present()) return "(none)";

  return monsoon::map_onto<std::string>(*v.get(),
      [](bool b) {
        return (b ? "true" : "false");
      },
      [](signed_type v) {
        return to_string(v);
      },
      [](unsigned_type v) {
        return to_string(v);
      },
      [](fp_type v) {
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
