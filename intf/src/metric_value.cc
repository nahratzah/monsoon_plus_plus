#include <monsoon/metric_value.h>
#include <monsoon/config_support.h>
#include <cmath>
#include <ostream>
#include <cassert>

namespace monsoon {
namespace {


template<typename... Fn>
class overload_t
: private Fn...
{
 public:
  template<typename... FnInit>
  constexpr overload_t(FnInit&&... fn)
  : Fn(std::forward<FnInit>(fn))...
  {}

  using Fn::operator()...;
};

template<typename... Fn>
constexpr auto overload(Fn&&... fn)
-> overload_t<std::decay_t<Fn>...> {
  return overload_t<std::decay_t<Fn>...>(std::forward<Fn>(fn)...);
}


} /* namespace monsoon::<anonymous> */


namespace metric_value_ops {


using unsigned_type = metric_value::unsigned_type;
using signed_type = metric_value::signed_type;
using fp_type = metric_value::fp_type;

constexpr auto signed_min = std::numeric_limits<signed_type>::min();
constexpr auto signed_max = std::numeric_limits<signed_type>::max();
constexpr auto abs_signed_min =
    static_cast<unsigned_type>(-(signed_min + 1)) + 1u;
constexpr auto abs_signed_max =
    static_cast<unsigned_type>(signed_max);
constexpr auto unsigned_min = std::numeric_limits<unsigned_type>::min();
constexpr auto unsigned_max = std::numeric_limits<unsigned_type>::max();

struct plus {
  monsoon_intf_local_
  metric_value operator()(unsigned_type x, unsigned_type y) const {
    auto sum = x + y;
    if (sum < y) // Overflow.
      return metric_value(static_cast<fp_type>(x) + static_cast<fp_type>(y));
    else
      return metric_value(sum);
  }

  monsoon_intf_local_
  metric_value operator()(signed_type x, signed_type y) const {
    assert(x < 0 && y < 0); // Enforced by metric_value constructor.

    if (std::numeric_limits<signed_type>::min() - x > y) // Underflow
      return metric_value(static_cast<fp_type>(x) + static_cast<fp_type>(y));
    return metric_value(x + y);
  }

  monsoon_intf_local_
  metric_value operator()(unsigned_type x, signed_type y) const {
    assert(y < 0); // Enforced by metric_value constructor.

    if (x > abs_signed_max) {
      // Use unsigned computation. Note that the overflow that may occur,
      // is defined behaviour and canceled out by the subtraction below.
      auto over_sum = (x + static_cast<unsigned_type>(y + signed_max));
      return metric_value(over_sum - abs_signed_max);
    }
    return metric_value(static_cast<signed_type>(x) + y);
  }

  monsoon_intf_local_
  metric_value operator()(signed_type x, unsigned_type y) const {
    return (*this)(y, x); // Commutative
  }

  monsoon_intf_local_
  metric_value operator()(fp_type x, fp_type y) const {
    return metric_value(x + y);
  }

  monsoon_intf_local_
  metric_value operator()(fp_type x, signed_type y) const {
    return metric_value(x + y);
  }

  monsoon_intf_local_
  metric_value operator()(fp_type x, unsigned_type y) const {
    return metric_value(x + y);
  }

  monsoon_intf_local_
  metric_value operator()(signed_type x, fp_type y) const {
    return metric_value(x + y);
  }

  monsoon_intf_local_
  metric_value operator()(unsigned_type x, fp_type y) const {
    return metric_value(x + y);
  }
};

struct minus {
  monsoon_intf_local_
  metric_value operator()(unsigned_type x, unsigned_type y) const {
    if (y > x) { // Underflow
      auto neg_sub = y - x;
      if (neg_sub > abs_signed_min) return metric_value(-static_cast<fp_type>(neg_sub));
      return metric_value(-static_cast<signed_type>(neg_sub - 1u) - 1);
    }
    return metric_value(x - y);
  }

  monsoon_intf_local_
  metric_value operator()(signed_type x, signed_type y) const {
    assert(x < 0 && y < 0); // Enforced by metric_value constructor.
    return metric_value(x - y);
  }

  monsoon_intf_local_
  metric_value operator()(unsigned_type x, signed_type y) const {
    assert(y < 0); // Enforced by metric_value constructor.
    auto abs_y = static_cast<unsigned_type>(-(y + 1)) + 1u;
    return plus()(x, abs_y);
  }

  monsoon_intf_local_
  metric_value operator()(signed_type x, unsigned_type y) const {
    assert(x < 0); // Enforced by metric_value constructor.
    auto abs_x = static_cast<unsigned_type>(-(x + 1)) + 1u;
    auto neg_sub = abs_x + y; // -(-x + y) == x - y
    if (neg_sub < y) // Overflow (of negated subtraction)
      return metric_value(static_cast<fp_type>(x) - static_cast<fp_type>(y));
    if (neg_sub > abs_signed_min) // Underflow
      return metric_value(-static_cast<fp_type>(neg_sub));
    if (neg_sub == 0) return metric_value(0u); // Since negation below can't handle this.
    return metric_value(-static_cast<signed_type>(neg_sub - 1u) - 1);
  }

  monsoon_intf_local_
  metric_value operator()(fp_type x, fp_type y) const {
    return metric_value(x - y);
  }

  monsoon_intf_local_
  metric_value operator()(fp_type x, signed_type y) const {
    return metric_value(x - y);
  }

  monsoon_intf_local_
  metric_value operator()(fp_type x, unsigned_type y) const {
    return metric_value(x - y);
  }

  monsoon_intf_local_
  metric_value operator()(signed_type x, fp_type y) const {
    return metric_value(x - y);
  }

  monsoon_intf_local_
  metric_value operator()(unsigned_type x, fp_type y) const {
    return metric_value(x - y);
  }
};

struct multiply {
  monsoon_intf_local_
  metric_value operator()(unsigned_type x, unsigned_type y) const {
    if (x == 0) return metric_value(0);
    if (unsigned_max / x < y) // Overflow
      return metric_value(static_cast<fp_type>(x) * static_cast<fp_type>(y));
    return metric_value(x * y);
  }

  monsoon_intf_local_
  metric_value operator()(signed_type x, signed_type y) const {
    assert(x < 0 && y < 0); // Enforced by metric_value constructor.
    auto abs_x = static_cast<unsigned_type>(-(x + 1)) + 1u;
    auto abs_y = static_cast<unsigned_type>(-(y + 1)) + 1u;
    return (*this)(abs_x, abs_y);
  }

  monsoon_intf_local_
  metric_value operator()(unsigned_type x, signed_type y) const {
    assert(y < 0); // Enforced by metric_value constructor.
    if (x == 0) return metric_value(0);
    auto abs_y = static_cast<unsigned_type>(-(y + 1)) + 1u;
    if (abs_signed_min / x < abs_y) // Overflow
      return metric_value(static_cast<fp_type>(x) * static_cast<fp_type>(y));

    auto neg_mul = x * abs_y;
    assert(neg_mul != 0); // Invariant, because x != 0 && y < 0
    return metric_value(-static_cast<signed_type>(neg_mul - 1u) - 1);
  }

  monsoon_intf_local_
  metric_value operator()(signed_type x, unsigned_type y) const {
    assert(x < 0); // Enforced by metric_value constructor.
    return (*this)(y, x); // Commutative
  }

  monsoon_intf_local_
  metric_value operator()(fp_type x, fp_type y) const {
    return metric_value(x * y);
  }

  monsoon_intf_local_
  metric_value operator()(fp_type x, signed_type y) const {
    return metric_value(x * y);
  }

  monsoon_intf_local_
  metric_value operator()(fp_type x, unsigned_type y) const {
    return metric_value(x * y);
  }

  monsoon_intf_local_
  metric_value operator()(signed_type x, fp_type y) const {
    return metric_value(x * y);
  }

  monsoon_intf_local_
  metric_value operator()(unsigned_type x, fp_type y) const {
    return metric_value(x - y);
  }
};

struct divide {
  monsoon_intf_local_
  metric_value operator()(unsigned_type x, unsigned_type y) const {
    if (y == 0) return metric_value(); // Divide-by-zero
    if (x % y != 0) // Remainder
      return (*this)(static_cast<fp_type>(x), static_cast<fp_type>(y));
    return metric_value(x / y);
  }

  monsoon_intf_local_
  metric_value operator()(signed_type x, signed_type y) const {
    assert(x < 0 && y < 0); // Enforced by metric_value constructor.
    auto abs_x = static_cast<unsigned_type>(-(x + 1)) + 1u;
    auto abs_y = static_cast<unsigned_type>(-(y + 1)) + 1u;
    return (*this)(abs_x, abs_y);
  }

  monsoon_intf_local_
  metric_value operator()(unsigned_type x, signed_type y) const {
    assert(y < 0); // Enforced by metric_value constructor.
    if (x == 0) return metric_value(0);
    auto abs_y = static_cast<unsigned_type>(-(y + 1)) + 1u;
    if (x % abs_y != 0) // Remainder
      return (*this)(static_cast<fp_type>(x), static_cast<fp_type>(y));

    auto neg_div = x / abs_y;
    if (neg_div > abs_signed_min) // Overflow
      return metric_value(-static_cast<fp_type>(neg_div));
    return metric_value(-static_cast<signed_type>(neg_div - 1u) - 1);
  }

  monsoon_intf_local_
  metric_value operator()(signed_type x, unsigned_type y) const {
    assert(x < 0); // Enforced by metric_value constructor.
    if (y == 0) return metric_value(); // Divide by zero
    auto abs_x = static_cast<unsigned_type>(-(x + 1)) + 1u;
    if (abs_x % y != 0) // Remainder
      return (*this)(static_cast<fp_type>(x), static_cast<fp_type>(y));

    auto neg_div = abs_x / y;
    if (neg_div > abs_signed_min) // Overflow
      return metric_value(-static_cast<fp_type>(neg_div));
    return metric_value(-static_cast<signed_type>(neg_div - 1u) - 1);
  }

  monsoon_intf_local_
  metric_value operator()(fp_type x, fp_type y) const {
    if (y == 0.0 || y == -0.0) return metric_value(); // Divide-by-zero
    return metric_value(x / y);
  }

  monsoon_intf_local_
  metric_value operator()(fp_type x, signed_type y) const {
    return (*this)(x, static_cast<fp_type>(y));
  }

  monsoon_intf_local_
  metric_value operator()(fp_type x, unsigned_type y) const {
    return (*this)(x, static_cast<fp_type>(y));
  }

  monsoon_intf_local_
  metric_value operator()(signed_type x, fp_type y) const {
    return (*this)(static_cast<fp_type>(x), y);
  }

  monsoon_intf_local_
  metric_value operator()(unsigned_type x, fp_type y) const {
    return (*this)(static_cast<fp_type>(x), y);
  }
};

struct modulo { // Worst idea ever to create this...
  monsoon_intf_local_
  metric_value operator()(unsigned_type x, unsigned_type y) const {
    if (y == 0) return metric_value(); // Divide by zero
    return metric_value(x % y);
  }

  monsoon_intf_local_
  metric_value operator()(signed_type x, signed_type y) const {
    assert(x < 0 && y < 0); // Enforced by metric_value constructor.
    return metric_value(x % y); // Defined behaviour since c++11
  }

  monsoon_intf_local_
  metric_value operator()(signed_type x, unsigned_type y) const {
    assert(x < 0); // Enforced by metric_value constructor.
    if (y == 0) return metric_value(); // Divide by zero
    return metric_value(x % y); // Defined behaviour since c++11
  }

  monsoon_intf_local_
  metric_value operator()(unsigned_type x, signed_type y) const {
    assert(y < 0); // Enforced by metric_value constructor.
    return metric_value(x % y); // Defined behaviour since c++11
  }

  monsoon_intf_local_
  metric_value operator()(fp_type x, fp_type y) const {
    if (y == 0.0 || y == -0.0) return metric_value(); // Divide-by-zero
    return metric_value(std::remainder(x, y));
  }

  monsoon_intf_local_
  metric_value operator()(fp_type x, signed_type y) const {
    return (*this)(x, static_cast<fp_type>(y));
  }

  monsoon_intf_local_
  metric_value operator()(fp_type x, unsigned_type y) const {
    return (*this)(x, static_cast<fp_type>(y));
  }

  monsoon_intf_local_
  metric_value operator()(signed_type x, fp_type y) const {
    return (*this)(static_cast<fp_type>(x), y);
  }

  monsoon_intf_local_
  metric_value operator()(unsigned_type x, fp_type y) const {
    return (*this)(static_cast<fp_type>(x), y);
  }
};

struct shift_left {
  monsoon_intf_local_
  metric_value operator()(unsigned_type x, unsigned_type y) const {
    if (y >= std::numeric_limits<unsigned_type>::digits
        || std::numeric_limits<unsigned_type>::max() >> y < x)
      return (*this)(static_cast<fp_type>(x), y);
    return metric_value(x << y);
  }

  monsoon_intf_local_
  metric_value operator()(signed_type x, unsigned_type y) const {
    assert(x < 0); // Enforced by metric_value constructor.

    if (y >= std::numeric_limits<signed_type>::digits - 1u
        || std::numeric_limits<signed_type>::min() >> y > x)
      return (*this)(static_cast<fp_type>(x), y);
    return metric_value(x << y);
  }

  monsoon_intf_local_
  metric_value operator()(signed_type x, signed_type y) const {
    assert(x < 0); // Enforced by metric_value constructor.
    assert(y < 0); // Enforced by metric_value constructor.

    if (y <= -std::numeric_limits<signed_type>::digits - 1u)
      return metric_value(0);
    return metric_value(x >> -y);
  }

  monsoon_intf_local_
  metric_value operator()(unsigned_type x, signed_type y) const {
    assert(y < 0); // Enforced by metric_value constructor.

    if (y <= -std::numeric_limits<unsigned_type>::digits)
      return metric_value(0);
    return metric_value(x >> -y);
  }

  template<typename X>
  monsoon_intf_local_
  metric_value operator()(X x, fp_type y) const {
    return metric_value(static_cast<fp_type>(x * std::pow(2.0l, y)));
  }

  monsoon_intf_local_
  metric_value operator()(fp_type x, unsigned_type y) const {
    long double tmp = x;
    while (y > std::numeric_limits<long>::max()) {
      tmp = scalblnl(tmp, std::numeric_limits<long>::max());
      y -= std::numeric_limits<long>::max();
    }
    return metric_value(
        static_cast<fp_type>(scalblnl(tmp, static_cast<long>(y))));
  }

  monsoon_intf_local_
  metric_value operator()(fp_type x, signed_type y) const {
    assert(y < 0); // Enforced by metric_value constructor.

    long double tmp = x;
    while (y < std::numeric_limits<long>::min()) {
      tmp = scalblnl(tmp, std::numeric_limits<long>::min());
      y -= std::numeric_limits<long>::min();
    }
    return metric_value(
        static_cast<fp_type>(scalblnl(tmp, static_cast<long>(y))));
  }
};

struct shift_right {
  monsoon_intf_local_
  metric_value operator()(unsigned_type x, unsigned_type y) const {
    if (y >= std::numeric_limits<unsigned_type>::digits)
      return metric_value(0);
    return metric_value(x >> y);
  }

  monsoon_intf_local_
  metric_value operator()(signed_type x, unsigned_type y) const {
    assert(x < 0); // Enforced by metric_value constructor.

    if (y >= std::numeric_limits<signed_type>::digits - 1u)
      return metric_value(0);
    return metric_value(x >> y);
  }

  monsoon_intf_local_
  metric_value operator()(signed_type x, signed_type y) const {
    assert(x < 0); // Enforced by metric_value constructor.
    assert(y < 0); // Enforced by metric_value constructor.

    if (y <= -std::numeric_limits<signed_type>::digits - 1u
        || std::numeric_limits<signed_type>::min() >> y > x)
      return (*this)(static_cast<fp_type>(x), y);
    return metric_value(x << -y);
  }

  monsoon_intf_local_
  metric_value operator()(unsigned_type x, signed_type y) const {
    assert(y < 0); // Enforced by metric_value constructor.

    if (y <= -std::numeric_limits<unsigned_type>::digits
        || std::numeric_limits<unsigned_type>::max() >> y < x)
      return (*this)(static_cast<fp_type>(x), y);
    return metric_value(x << -y);
  }

  template<typename X>
  monsoon_intf_local_
  metric_value operator()(X x, fp_type y) const {
    return metric_value(static_cast<fp_type>(x / std::pow(2.0l, y)));
  }

  monsoon_intf_local_
  metric_value operator()(fp_type x, unsigned_type y) const {
    long double tmp = x;
    while (y > std::numeric_limits<long>::max()) {
      tmp = scalblnl(tmp, -std::numeric_limits<long>::max());
      y -= std::numeric_limits<long>::max();
    }
    return metric_value(
        static_cast<fp_type>(scalblnl(tmp, static_cast<long>(y))));
  }

  monsoon_intf_local_
  metric_value operator()(fp_type x, signed_type y) const {
    assert(y < 0); // Enforced by metric_value constructor.

    long double tmp = x;
    while (y < -std::numeric_limits<long>::max()) {
      tmp = scalblnl(tmp, -std::numeric_limits<long>::max());
      y += std::numeric_limits<long>::max();
    }
    return metric_value(
        static_cast<fp_type>(scalblnl(tmp, static_cast<long>(y))));
  }
};


} /* namespace monsoon::metric_value_ops */


auto metric_value::operator==(const metric_value& other) const noexcept
->  bool {
  return value_ == other.value_;
}

auto metric_value::as_bool() const noexcept -> std::optional<bool> {
  return visit(
      [](const auto& v) -> std::optional<bool> {
        using v_type = std::decay_t<decltype(v)>;
        static_assert(std::is_same_v<empty, v_type>
           || std::is_same_v<bool, v_type>
           || std::is_same_v<signed_type, v_type>
           || std::is_same_v<unsigned_type, v_type>
           || std::is_same_v<fp_type, v_type>
           || std::is_same_v<histogram, v_type>
           || std::is_same_v<std::string, v_type>,
           "Programmer error: type deduction.");

        if constexpr(std::is_same_v<bool, v_type>)
          return v;
        else if constexpr(std::is_same_v<signed_type, v_type>)
          return v != 0;
        else if constexpr(std::is_same_v<unsigned_type, v_type>)
          return v != 0u;
        else if constexpr(std::is_same_v<fp_type, v_type>)
          return v != 0.0l && v != -0.0l;
        else if constexpr(std::is_same_v<histogram, v_type>)
          return !v.empty();
        else
          return {};
      },
      value_);
}

auto metric_value::as_number() const noexcept
->  std::optional<std::variant<signed_type, unsigned_type, fp_type>> {
  using result_types = std::variant<signed_type, unsigned_type, fp_type>;

  return visit(
      [](const auto& v) -> std::optional<result_types> {
        using v_type = std::decay_t<decltype(v)>;
        static_assert(std::is_same_v<empty, v_type>
           || std::is_same_v<bool, v_type>
           || std::is_same_v<signed_type, v_type>
           || std::is_same_v<unsigned_type, v_type>
           || std::is_same_v<fp_type, v_type>
           || std::is_same_v<histogram, v_type>
           || std::is_same_v<std::string, v_type>,
           "Programmer error: type deduction.");

        if constexpr(std::is_same_v<bool, v_type>)
          return result_types(std::in_place_type<unsigned_type>, (v ? 1 : 0));
        else if constexpr(std::is_same_v<signed_type, v_type>)
          return result_types(std::in_place_type<signed_type>, v);
        else if constexpr(std::is_same_v<unsigned_type, v_type>)
          return result_types(std::in_place_type<unsigned_type>, v);
        else if constexpr(std::is_same_v<fp_type, v_type>)
          return result_types(std::in_place_type<fp_type>, v);
        else
          return {};
      },
      value_);
}

auto metric_value::as_number_or_histogram() const noexcept
->  std::optional<std::variant<signed_type, unsigned_type, fp_type, histogram>> {
  using result_types = std::variant<signed_type, unsigned_type, fp_type, histogram>;

  return visit(
      [](const auto& v) -> std::optional<result_types> {
        using v_type = std::decay_t<decltype(v)>;
        static_assert(std::is_same_v<empty, v_type>
           || std::is_same_v<bool, v_type>
           || std::is_same_v<signed_type, v_type>
           || std::is_same_v<unsigned_type, v_type>
           || std::is_same_v<fp_type, v_type>
           || std::is_same_v<histogram, v_type>
           || std::is_same_v<std::string, v_type>,
           "Programmer error: type deduction.");

        if constexpr(std::is_same_v<bool, v_type>)
          return result_types(std::in_place_type<unsigned_type>, (v ? 1 : 0));
        else if constexpr(std::is_same_v<signed_type, v_type>)
          return result_types(std::in_place_type<signed_type>, v);
        else if constexpr(std::is_same_v<unsigned_type, v_type>)
          return result_types(std::in_place_type<unsigned_type>, v);
        else if constexpr(std::is_same_v<fp_type, v_type>)
          return result_types(std::in_place_type<fp_type>, v);
        else if constexpr(std::is_same_v<histogram, v_type>)
          return result_types(std::in_place_type<histogram>, v);
        else
          return {};
      },
      value_);
}

auto metric_value::as_string() const -> std::optional<std::string> {
  using std::to_string;

  return visit(
      [](const auto& v) -> std::optional<std::string> {
        using v_type = std::decay_t<decltype(v)>;
        static_assert(std::is_same_v<empty, v_type>
           || std::is_same_v<bool, v_type>
           || std::is_same_v<signed_type, v_type>
           || std::is_same_v<unsigned_type, v_type>
           || std::is_same_v<fp_type, v_type>
           || std::is_same_v<histogram, v_type>
           || std::is_same_v<std::string, v_type>,
           "Programmer error: type deduction.");

        if constexpr(std::is_same_v<empty, v_type>)
          return {};
        if constexpr(std::is_same_v<bool, v_type>)
          return (v ? "true" : "false");
        if constexpr(std::is_same_v<signed_type, v_type>)
          return to_string(v);
        if constexpr(std::is_same_v<unsigned_type, v_type>)
          return to_string(v);
        if constexpr(std::is_same_v<fp_type, v_type>)
          return to_string(v);
        if constexpr(std::is_same_v<std::string, v_type>)
          return v;
        if constexpr(std::is_same_v<histogram, v_type>)
          return {};
      },
      value_);
}

auto metric_value::before(const metric_value& x, const metric_value& y)
    noexcept
->  bool {
  if (x.value_.index() != y.value_.index())
    return x.value_.index() < y.value_.index();

  return visit(
      [&y](const auto& v) {
        using v_type = std::decay_t<decltype(v)>;
        static_assert(std::is_same_v<empty, v_type>
           || std::is_same_v<bool, v_type>
           || std::is_same_v<signed_type, v_type>
           || std::is_same_v<unsigned_type, v_type>
           || std::is_same_v<fp_type, v_type>
           || std::is_same_v<histogram, v_type>
           || std::is_same_v<std::string, v_type>,
           "Programmer error: type deduction.");

        if constexpr(std::is_same_v<empty, v_type>)
          return false; // empty() < empty() -> false
        else if constexpr(std::is_same_v<histogram, v_type>)
          return histogram::before(v, std::get<histogram>(y.value_));
        else
          return v < std::get<v_type>(y.value_);
      },
      x.value_);
}


auto operator!(const metric_value& x) noexcept -> metric_value {
  auto v = x.as_bool();
  return (v.has_value() ? metric_value(v.value()) : metric_value());
}

auto operator&&(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  auto x_bool = x.as_bool();
  if (!x_bool.has_value()) return metric_value();
  auto y_bool = y.as_bool();
  if (!y_bool.has_value()) return metric_value();
  return metric_value(x_bool.value() && y_bool.value());
}

auto operator||(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  auto x_bool = x.as_bool();
  if (!x_bool.has_value()) return metric_value();
  auto y_bool = y.as_bool();
  if (!y_bool.has_value()) return metric_value();
  return metric_value(x_bool.value() || y_bool.value());
}

auto operator-(const metric_value& x) noexcept -> metric_value {
  using unsigned_type = metric_value::unsigned_type;
  using signed_type = metric_value::signed_type;
  using fp_type = metric_value::fp_type;

  auto x_num = x.as_number_or_histogram();
  if (!x_num.has_value()) return metric_value();

  return visit(
      [](auto&& v) -> metric_value {
        using v_type = std::decay_t<decltype(v)>;

        if constexpr(std::is_same_v<signed_type, v_type>) {
          assert(v < 0); // Enforced by metric_value constructors.
          // Do signed->unsigned conversion in a way to avoid overflow.
          return metric_value(static_cast<unsigned_type>(-(v + 1)) + 1u);
        } else if constexpr(std::is_same_v<unsigned_type, v_type>) {
          // Handle cases where v is too large to convert to negative integral.
          if (v > static_cast<unsigned_type>(-(std::numeric_limits<signed_type>::min() + 1)) + 1u)
            return metric_value(-static_cast<fp_type>(v));
          // Handle cases where v would overflow during unsigned->signed conversion.
          if (v > static_cast<unsigned_type>(std::numeric_limits<signed_type>::max()))
            return metric_value(-static_cast<signed_type>(v - 1u) - 1);
          // Default case.
          return metric_value(-static_cast<signed_type>(v));
        } else {
          return metric_value(-std::move(v)); // double, histogram
        }
      },
      std::move(x_num).value());
}

auto operator+(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  auto x_num = x.as_number_or_histogram();
  if (!x_num.has_value()) return {};
  auto y_num = y.as_number_or_histogram();
  if (!y_num.has_value()) return {};

  return visit(
      overload(
          [](auto&& x_val, auto&& y_val) {
            using x_type = decltype(x_val);
            using y_type = decltype(y_val);

            if constexpr(std::is_same_v<histogram, std::decay_t<x_type>> && std::is_same_v<histogram, std::decay_t<y_type>>)
              return metric_value(std::forward<x_type>(x_val) + std::forward<y_type>(y_val));
            else
              return metric_value();
          },
          metric_value_ops::plus()), // Signed, unsigned types
      std::move(x_num).value(), std::move(y_num).value());
}

auto operator-(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  auto x_num = x.as_number_or_histogram();
  if (!x_num.has_value()) return {};
  auto y_num = y.as_number_or_histogram();
  if (!y_num.has_value()) return {};

  return visit(
      overload(
          [](auto&& x_val, auto&& y_val) {
            using x_type = decltype(x_val);
            using y_type = decltype(y_val);

            if constexpr(std::is_same_v<histogram, std::decay_t<x_type>> && std::is_same_v<histogram, std::decay_t<y_type>>)
              return metric_value(std::forward<x_type>(x_val) - std::forward<y_type>(y_val));
            else
              return metric_value();
          },
          metric_value_ops::minus()), // Signed, unsigned types
      std::move(x_num).value(), std::move(y_num).value());
}

auto operator*(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  auto x_num = x.as_number_or_histogram();
  if (!x_num.has_value()) return {};
  auto y_num = y.as_number_or_histogram();
  if (!y_num.has_value()) return {};

  return visit(
      overload(
          [](auto&& x_val, auto&& y_val) {
            using x_type = decltype(x_val);
            using y_type = decltype(y_val);

            if constexpr(
                (std::is_same_v<histogram, std::decay_t<x_type>>
                 && !std::is_same_v<histogram, std::decay_t<y_type>>)
                ||
                (!std::is_same_v<histogram, std::decay_t<x_type>>
                 && std::is_same_v<histogram, std::decay_t<y_type>>))
              return metric_value(std::forward<x_type>(x_val) * std::forward<y_type>(y_val));
            else
              return metric_value();
          },
          metric_value_ops::multiply()), // Signed, unsigned types
      std::move(x_num).value(), std::move(y_num).value());
}

auto operator/(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  auto x_num = x.as_number_or_histogram();
  if (!x_num.has_value()) return {};
  auto y_num = y.as_number_or_histogram();
  if (!y_num.has_value()) return {};

  return visit(
      overload(
          [](auto&& x_val, auto&& y_val) {
            using x_type = decltype(x_val);
            using y_type = decltype(y_val);

            if constexpr(std::is_same_v<histogram, std::decay_t<x_type>>
                && !std::is_same_v<histogram, std::decay_t<y_type>>)
              return metric_value(std::forward<x_type>(x_val) / std::forward<y_type>(y_val));
            else
              return metric_value();
          },
          metric_value_ops::divide()), // Signed, unsigned types
      std::move(x_num).value(), std::move(y_num).value());
}

auto operator%(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  auto x_num = x.as_number();
  if (!x_num.has_value()) return {};
  auto y_num = y.as_number();
  if (!y_num.has_value()) return {};

  return visit(
      metric_value_ops::modulo(), // Signed, unsigned types
      std::move(x_num).value(), std::move(y_num).value());
}

auto operator<<(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  auto x_num = x.as_number();
  if (!x_num.has_value()) return {};
  auto y_num = y.as_number();
  if (!y_num.has_value()) return {};

  return visit(
      metric_value_ops::shift_left(), // Signed, unsigned types
      std::move(x_num).value(), std::move(y_num).value());
}

auto operator>>(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  auto x_num = x.as_number();
  if (!x_num.has_value()) return {};
  auto y_num = y.as_number();
  if (!y_num.has_value()) return {};

  return visit(
      metric_value_ops::shift_right(), // Signed, unsigned types
      std::move(x_num).value(), std::move(y_num).value());
}

auto operator<<(std::ostream& out, const metric_value& v) -> std::ostream& {
  visit(
      [&out](const auto& v) {
        using v_type = std::decay_t<decltype(v)>;

        if constexpr(std::is_same_v<metric_value::empty, v_type>)
          out << "(none)";
        else if constexpr(std::is_same_v<bool, v_type>)
          out << (v ? "true" : "false");
        else
          out << v;
      },
      v.get());
  return out;
}

auto equal(const metric_value& x, const metric_value& y) noexcept
->  metric_value {
  using empty = metric_value::empty;
  using signed_type = metric_value::signed_type;
  using unsigned_type = metric_value::unsigned_type;
  using fp_type = metric_value::fp_type;

  return visit(
      overload(
          // empty
          [](const empty&, const auto&) {
            return metric_value();
          },
          [](const auto&, const empty&) {
            return metric_value();
          },
          [](const empty&, const empty&) { // Resolve ambiguity
            return metric_value();
          },
          // bool == ???
          [](bool x_val, bool y_val) {
            return metric_value(x_val == y_val);
          },
          [](bool x_val, signed_type y_val) {
            return metric_value(x_val == y_val);
          },
          [](bool x_val, unsigned_type y_val) {
            return metric_value(x_val == y_val);
          },
          [](bool x_val, fp_type y_val) {
            return metric_value(x_val == y_val);
          },
          [](bool x_val, const std::string& y_val) {
            return metric_value();
          },
          [](bool x_val, const histogram& y_val) {
            return metric_value();
          },
          // signed_type == ???
          [](signed_type x_val, bool y_val) {
            return metric_value(x_val == y_val);
          },
          [](signed_type x_val, signed_type y_val) {
            return metric_value(x_val == y_val);
          },
          [](signed_type x_val, unsigned_type y_val) {
            assert(x_val < 0); // Enforced by constructor.
            return metric_value(false); // Domain exclusion
          },
          [](signed_type x_val, fp_type y_val) {
            return metric_value(x_val == y_val);
          },
          [](signed_type x_val, const std::string& y_val) {
            return metric_value();
          },
          [](signed_type x_val, const histogram& y_val) {
            return metric_value();
          },
          // unsigned_type == ???
          [](unsigned_type x_val, bool y_val) {
            return metric_value(x_val == y_val);
          },
          [](unsigned_type x_val, signed_type y_val) {
            assert(y_val < 0); // Enforced by constructor.
            return metric_value(false); // Domain exclusion
          },
          [](unsigned_type x_val, unsigned_type y_val) {
            return metric_value(x_val == y_val);
          },
          [](unsigned_type x_val, fp_type y_val) {
            return metric_value(x_val == y_val);
          },
          [](unsigned_type x_val, const std::string& y_val) {
            return metric_value();
          },
          [](unsigned_type x_val, const histogram& y_val) {
            return metric_value();
          },
          // fp_type == ???
          [](fp_type x_val, bool y_val) {
            return metric_value(x_val == y_val);
          },
          [](fp_type x_val, signed_type y_val) {
            return metric_value(x_val == y_val);
          },
          [](fp_type x_val, unsigned_type y_val) {
            return metric_value(x_val == y_val);
          },
          [](fp_type x_val, fp_type y_val) {
            return metric_value(x_val == y_val);
          },
          [](fp_type x_val, const std::string& y_val) {
            return metric_value();
          },
          [](fp_type x_val, const histogram& y_val) {
            return metric_value();
          },
          // std::string == ???
          [](const std::string& x_val, bool y_val) {
            return metric_value();
          },
          [](const std::string& x_val, signed_type y_val) {
            return metric_value();
          },
          [](const std::string& x_val, unsigned_type y_val) {
            return metric_value();
          },
          [](const std::string& x_val, fp_type y_val) {
            return metric_value();
          },
          [](const std::string& x_val, const std::string& y_val) {
            return metric_value(x_val == y_val);
          },
          [](const std::string& x_val, const histogram& y_val) {
            return metric_value();
          },
          // histogram == ???
          [](const histogram& x_val, bool y_val) {
            return metric_value();
          },
          [](const histogram& x_val, signed_type y_val) {
            return metric_value();
          },
          [](const histogram& x_val, unsigned_type y_val) {
            return metric_value();
          },
          [](const histogram& x_val, fp_type y_val) {
            return metric_value();
          },
          [](const histogram& x_val, const std::string& y_val) {
            return metric_value();
          },
          [](const histogram& x_val, const histogram& y_val) {
            return metric_value(x_val == y_val);
          }),
      x.get(), y.get());
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
  if (!x_num.has_value()) return metric_value();
  const auto y_num = y.as_number();
  if (!y_num.has_value()) return metric_value();

  return visit(
      overload(
          // fp_type
          [](fp_type x_val, auto y_val) {
            return metric_value(x_val < y_val);
          },
          [](auto x_val, fp_type y_val) {
            return metric_value(x_val < y_val);
          },
          [](fp_type x_val, fp_type y_val) { // Resolve ambiguity
            return metric_value(x_val < y_val);
          },
          // signed_type == ???
          [](signed_type x_val, signed_type y_val) {
            return metric_value(x_val < y_val);
          },
          [](signed_type x_val, unsigned_type y_val) {
            assert(x_val < 0); // Enforced by constructor.
            return metric_value(false); // Domain exclusion
          },
          // unsigned_type == ???
          [](unsigned_type x_val, signed_type y_val) {
            assert(y_val < 0); // Enforced by constructor.
            return metric_value(false); // Domain exclusion
          },
          [](unsigned_type x_val, unsigned_type y_val) {
            return metric_value(x_val < y_val);
          }),
      std::move(x_num).value(), std::move(y_num).value());
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

auto to_string(const monsoon::metric_value& v) -> std::string {
  using std::to_string; // ADL

  return visit(
      [](const auto& v) -> std::string {
        using v_type = std::decay_t<decltype(v)>;

        if constexpr(std::is_same_v<metric_value::empty, v_type>)
          return "(none)";
        else if constexpr(std::is_same_v<bool, v_type>)
          return (v ? "true" : "false");
        else if constexpr(std::is_same_v<std::string, v_type>)
          return v;
        else
          return to_string(v); // ADL
      },
      v.get());
}


} /* namespace monsoon */


namespace std {


auto std::hash<monsoon::metric_value>::operator()(
    const monsoon::metric_value& v) const noexcept
->  size_t {
  return std::hash<monsoon::metric_value::types>()(v.get());
}


} /* namespace std */
