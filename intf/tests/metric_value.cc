#include <monsoon/metric_value.h>
#include <monsoon/histogram.h>
#include <string_view>
#include <utility>
#include <tuple>
#include <sstream>
#include "UnitTest++/UnitTest++.h"

using namespace monsoon;
using namespace std::string_view_literals;
using namespace std::string_literals;

template<typename T, typename... Args>
inline metric_value::types build_mv_types(Args&&... args) {
  return metric_value::types(
      std::in_place_type<T>,
      std::forward<Args>(args)...);
}

namespace std {

inline auto operator<<(std::ostream& out, const metric_value::types& t)
-> std::ostream& {
  out << "("
      << t.index()
      << " -> ";
  std::visit(
      [&out](const auto& v) { out << v; },
      t);
  return out << ")";
}

} /* namespace std */

static_assert(
    sizeof(metric_value::unsigned_type) == 8,
    "metric_value::unsigned_type has wrong size");
static_assert(
    sizeof(metric_value::signed_type) == 8,
    "metric_value::signed_type has wrong size");
static_assert(
    sizeof(metric_value::fp_type) >= sizeof(double),
    "metric_value::fp_type has wrong size");

TEST(constructor) {
  // Empty value.
  CHECK_EQUAL(
      build_mv_types<metric_value::empty>(),
      metric_value().get());

  // Boolean value.
  CHECK_EQUAL(
      build_mv_types<bool>(true),
      metric_value(true).get());
  CHECK_EQUAL(
      build_mv_types<bool>(false),
      metric_value(false).get());

  // Unsigned integer value.
  CHECK_EQUAL(
      build_mv_types<metric_value::unsigned_type>(0),
      metric_value(std::int16_t(0)).get());
  CHECK_EQUAL(
      build_mv_types<metric_value::unsigned_type>(0),
      metric_value(std::uint16_t(0)).get());
  CHECK_EQUAL(
      build_mv_types<metric_value::unsigned_type>(1),
      metric_value(std::intmax_t(1)).get());
  CHECK_EQUAL(
      build_mv_types<metric_value::unsigned_type>(1),
      metric_value(std::uintmax_t(1)).get());
  CHECK_EQUAL(
      build_mv_types<metric_value::unsigned_type>(17),
      metric_value(17l).get());
  CHECK_EQUAL(
      build_mv_types<metric_value::unsigned_type>(17),
      metric_value(17ul).get());
  CHECK_EQUAL(
      build_mv_types<metric_value::unsigned_type>(std::numeric_limits<metric_value::unsigned_type>::max()),
      metric_value(std::numeric_limits<metric_value::unsigned_type>::max()).get());

  // Signed integer value.
  CHECK_EQUAL(
      build_mv_types<metric_value::signed_type>(-1),
      metric_value(-1).get());
  CHECK_EQUAL(
      build_mv_types<metric_value::signed_type>(-17),
      metric_value(-17).get());
  CHECK_EQUAL(
      build_mv_types<metric_value::signed_type>(std::numeric_limits<metric_value::signed_type>::min()),
      metric_value(std::numeric_limits<metric_value::signed_type>::min()).get());

  // Floating point value.
  CHECK_EQUAL(
      build_mv_types<metric_value::fp_type>(42.1),
      metric_value(42.1).get());
  CHECK_EQUAL(
      build_mv_types<metric_value::fp_type>(-42.1),
      metric_value(-42.1).get());
  CHECK_EQUAL(
      build_mv_types<metric_value::fp_type>(1e100),
      metric_value(1e100).get());

  // String value.
  CHECK_EQUAL(
      build_mv_types<std::string>(u8"foo"),
      metric_value(u8"foo").get()); // Using char*
  CHECK_EQUAL(
      build_mv_types<std::string>(u8"foo"),
      metric_value(u8"foo"s).get()); // Using std::string
  CHECK_EQUAL(
      build_mv_types<std::string>(u8"foo"),
      metric_value(u8"foo"sv).get()); // Using std::string_view

  // Histogram value.
  CHECK_EQUAL(
      build_mv_types<histogram>(histogram::parse(R"([])")),
      metric_value(histogram::parse(R"([])")).get());
  CHECK_EQUAL(
      build_mv_types<histogram>(histogram::parse(R"([0 .. 1 = 2])")),
      metric_value(histogram::parse(R"([0 .. 1 = 2])")).get());
}

TEST(equality) {
  // Empty value.
  CHECK_EQUAL(true, metric_value() == metric_value());
  CHECK_EQUAL(false, metric_value() != metric_value());

  // Boolean value.
  CHECK_EQUAL(true, metric_value(true) == metric_value(true));
  CHECK_EQUAL(true, metric_value(false) == metric_value(false));
  CHECK_EQUAL(false, metric_value(true) == metric_value(false));
  CHECK_EQUAL(false, metric_value(false) == metric_value(true));

  CHECK_EQUAL(false, metric_value(true) != metric_value(true));
  CHECK_EQUAL(false, metric_value(false) != metric_value(false));
  CHECK_EQUAL(true, metric_value(true) != metric_value(false));
  CHECK_EQUAL(true, metric_value(false) != metric_value(true));

  // Unsigned value.
  CHECK_EQUAL(true, metric_value(0) == metric_value(0));
  CHECK_EQUAL(true, metric_value(42) == metric_value(42));
  CHECK_EQUAL(false, metric_value(42) == metric_value(41));

  CHECK_EQUAL(false, metric_value(0) != metric_value(0));
  CHECK_EQUAL(false, metric_value(42) != metric_value(42));
  CHECK_EQUAL(true, metric_value(42) != metric_value(41));

  // Signed value.
  CHECK_EQUAL(true, metric_value(-42) == metric_value(-42));
  CHECK_EQUAL(false, metric_value(-42) == metric_value(-41));

  CHECK_EQUAL(false, metric_value(-42) != metric_value(-42));
  CHECK_EQUAL(true, metric_value(-42) != metric_value(-41));

  // Floating point value.
  CHECK_EQUAL(true, metric_value(0.0) == metric_value(0.0));
  CHECK_EQUAL(true, metric_value(-0.0) == metric_value(-0.0));
  CHECK_EQUAL(true, metric_value(0.0) == metric_value(-0.0));
  CHECK_EQUAL(true, metric_value(-0.0) == metric_value(0.0));

  CHECK_EQUAL(false, metric_value(0.0) != metric_value(0.0));
  CHECK_EQUAL(false, metric_value(-0.0) != metric_value(-0.0));
  CHECK_EQUAL(false, metric_value(0.0) != metric_value(-0.0));
  CHECK_EQUAL(false, metric_value(-0.0) != metric_value(0.0));

  // Floating point value with unsigned value.
  CHECK_EQUAL(true, metric_value(0.0) == metric_value(0));
  CHECK_EQUAL(true, metric_value(-0.0) == metric_value(0));
  CHECK_EQUAL(true, metric_value(42.0) == metric_value(42));
  CHECK_EQUAL(false, metric_value(42.1) == metric_value(42));
  CHECK_EQUAL(false, metric_value(41.9) == metric_value(42));

  CHECK_EQUAL(false, metric_value(0.0) != metric_value(0));
  CHECK_EQUAL(false, metric_value(-0.0) != metric_value(0));
  CHECK_EQUAL(false, metric_value(42.0) != metric_value(42));
  CHECK_EQUAL(true, metric_value(42.1) != metric_value(42));
  CHECK_EQUAL(true, metric_value(41.9) != metric_value(42));

  // Floating point value with signed value.
  CHECK_EQUAL(true, metric_value(-42.0) == metric_value(-42));

  // String value.
  CHECK_EQUAL(true, metric_value("foo") == metric_value("foo"));
  CHECK_EQUAL(false, metric_value("foo") == metric_value("bar"));
  CHECK_EQUAL(false, metric_value("foo") != metric_value("foo"));
  CHECK_EQUAL(true, metric_value("foo") != metric_value("bar"));

  // Histogram value.
  CHECK_EQUAL(true,
      metric_value(histogram::parse(R"([])")) ==
      metric_value(histogram::parse(R"([])")));
  CHECK_EQUAL(false,
      metric_value(histogram::parse(R"([1..2=3])")) ==
      metric_value(histogram::parse(R"([1..2=2])")));

  CHECK_EQUAL(false,
      metric_value(histogram::parse(R"([])")) !=
      metric_value(histogram::parse(R"([])")));
  CHECK_EQUAL(true,
      metric_value(histogram::parse(R"([1..2=3])")) !=
      metric_value(histogram::parse(R"([1..2=2])")));
}

TEST(hash) {
  std::hash<metric_value> h;

  // Boolean value.
  CHECK_EQUAL(
      h(metric_value(true)), h(metric_value(true)));
  CHECK_EQUAL(
      h(metric_value(false)), h(metric_value(false)));

  // Unsigned value.
  CHECK_EQUAL(
      h(metric_value(0)), h(metric_value(0)));
  CHECK_EQUAL(
      h(metric_value(7)), h(metric_value(7)));

  // Signed value.
  CHECK_EQUAL(
      h(metric_value(-1)), h(metric_value(-1)));
  CHECK_EQUAL(
      h(metric_value(-100)), h(metric_value(-100)));

  // Floating point value.
  CHECK_EQUAL(
      h(metric_value(0.0)), h(metric_value(0.0)));
  CHECK_EQUAL(
      h(metric_value(-0.0)), h(metric_value(0.0)));
  CHECK_EQUAL(
      h(metric_value(0.0)), h(metric_value(-0.0)));
  CHECK_EQUAL(
      h(metric_value(-0.0)), h(metric_value(-0.0)));
  CHECK_EQUAL(
      h(metric_value(1e100)), h(metric_value(1e100)));

  // Floating point value with unsigned value.
  CHECK_EQUAL(
      h(metric_value(17.0)), h(metric_value(17)));
  CHECK_EQUAL(
      h(metric_value(0.0)), h(metric_value(0)));
  CHECK_EQUAL(
      h(metric_value(-0.0)), h(metric_value(0)));

  // Floating point value with signed value.
  CHECK_EQUAL(
      h(metric_value(-17.0)), h(metric_value(-17)));

  // String value.
  CHECK_EQUAL(h(metric_value("foo")), h(metric_value("foo")));
  CHECK_EQUAL(h(metric_value("bar")), h(metric_value("bar")));

  // Histogram value.
  CHECK_EQUAL(
      h(metric_value(histogram::parse(R"([])"))),
      h(metric_value(histogram::parse(R"([])"))));
  CHECK_EQUAL(
      h(metric_value(histogram::parse(R"([1..2=3])"))),
      h(metric_value(histogram::parse(R"([1..2=3])"))));
}

TEST(before) {
  // Boolean value.
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(false), metric_value(false)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(true), metric_value(true)));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(false), metric_value(true)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(true), metric_value(false)));

  // Boolean and numeric value.
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(false), metric_value(0)));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(true), metric_value(0)));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(false), metric_value(1)));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(true), metric_value(1)));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(false), metric_value(-1)));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(true), metric_value(-1)));

  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(0), metric_value(false)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(0), metric_value(true)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(1), metric_value(false)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(1), metric_value(true)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(-1), metric_value(false)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(-1), metric_value(true)));

  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(false), metric_value(0.0)));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(true), metric_value(-0.0)));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(false), metric_value(-0.0)));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(true), metric_value(0.0)));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(false), metric_value(1.0)));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(true), metric_value(1.0)));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(false), metric_value(-1.0)));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(true), metric_value(-1.0)));

  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(0.0), metric_value(false)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(0.0), metric_value(true)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(-0.0), metric_value(false)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(-0.0), metric_value(true)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(1.0), metric_value(false)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(1.0), metric_value(true)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(-1.0), metric_value(false)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(-1.0), metric_value(true)));

  // Unsigned value.
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(0), metric_value(0)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(1), metric_value(1)));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(0), metric_value(1)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(1), metric_value(0)));

  // Signed value.
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(-1), metric_value(-1)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(-100), metric_value(-100)));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(-100), metric_value(-1)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(-1), metric_value(-100)));

  // Floating point value.
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(0.0), metric_value(0.0)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(-0.0), metric_value(-0.0)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(1.0), metric_value(1.0)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(-1.0), metric_value(-1.0)));

  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(0.0), metric_value(-0.0)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(-0.0), metric_value(0.0)));

  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(-1.0), metric_value(-0.0)));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(-1.0), metric_value(0.0)));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(0.0), metric_value(1.0)));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(-1.0), metric_value(1.0)));

  // Signed and unsigned value.
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(-1), metric_value(1)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(1), metric_value(-1)));

  // Floating point value and integral value.
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(0.0), metric_value(0)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(0), metric_value(0.0)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(-0.0), metric_value(0)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(0), metric_value(-0.0)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(1), metric_value(1.0)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(1.0), metric_value(1)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(-1), metric_value(-1.0)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(-1.0), metric_value(-1)));

  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(0.0), metric_value(1)));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(-0.0), metric_value(1)));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(1.0), metric_value(2)));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(-2.0), metric_value(-1)));

  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(1), metric_value(0.0)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(1), metric_value(-0.0)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(2), metric_value(1.0)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(-1), metric_value(-2.0)));

  // String value.
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value("foo"), metric_value("foo")));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value("foo"), metric_value("bar")));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value("bar"), metric_value("foo")));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(""), metric_value("foo")));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value("foobar"), metric_value("foo")));

  // Strings come after empty, booleans, numerics.
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(), metric_value("")));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(false), metric_value("")));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(true), metric_value("")));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(-1), metric_value("")));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(0), metric_value("")));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(1), metric_value("")));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(0.0), metric_value("")));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(-0.0), metric_value("")));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(1.0), metric_value("")));
  CHECK_EQUAL(
      true,
      metric_value::before(metric_value(-1.0), metric_value("")));

  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(""), metric_value()));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(""), metric_value(false)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(""), metric_value(true)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(""), metric_value(-1)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(""), metric_value(0)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(""), metric_value(1)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(""), metric_value(0.0)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(""), metric_value(-0.0)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(""), metric_value(1.0)));
  CHECK_EQUAL(
      false,
      metric_value::before(metric_value(""), metric_value(-1.0)));

  // Histogram value (histogram ordering is used).
  CHECK_EQUAL(
      histogram::before(
          histogram::parse("[]"),
          histogram::parse("[]")),
      metric_value::before(
          metric_value(histogram::parse("[]")),
          metric_value(histogram::parse("[]"))));
  CHECK_EQUAL(
      histogram::before(
          histogram::parse("[]"),
          histogram::parse("[0..1=1]")),
      metric_value::before(
          metric_value(histogram::parse("[]")),
          metric_value(histogram::parse("[0..1=1]"))));
  CHECK_EQUAL(
      histogram::before(
          histogram::parse("[]"),
          histogram::parse("[0..1=-1]")),
      metric_value::before(
          metric_value(histogram::parse("[]")),
          metric_value(histogram::parse("[0..1=-1]"))));

  CHECK_EQUAL(
      histogram::before(
          histogram::parse("[0..1=1]"),
          histogram::parse("[]")),
      metric_value::before(
          metric_value(histogram::parse("[0..1=1]")),
          metric_value(histogram::parse("[]"))));
  CHECK_EQUAL(
      histogram::before(
          histogram::parse("[0..1=-1]"),
          histogram::parse("[]")),
      metric_value::before(
          metric_value(histogram::parse("[0..1=-1]")),
          metric_value(histogram::parse("[]"))));
}

TEST(to_string) {
  CHECK_EQUAL("(none)", to_string(metric_value()));

  CHECK_EQUAL("false", to_string(metric_value(false)));
  CHECK_EQUAL("true", to_string(metric_value(true)));

  CHECK_EQUAL("0", to_string(metric_value(0)));
  CHECK_EQUAL("17", to_string(metric_value(17)));

  CHECK_EQUAL("-17", to_string(metric_value(-17)));

  CHECK_EQUAL(std::to_string(double(0.0)), to_string(metric_value(0.0)));
  CHECK_EQUAL(std::to_string(double(-0.0)), to_string(metric_value(-0.0)));
  CHECK_EQUAL(std::to_string(double(1.0)), to_string(metric_value(1.0)));
  CHECK_EQUAL(std::to_string(double(1e100)), to_string(metric_value(1e100)));

  CHECK_EQUAL(u8R"("foobar")", to_string(metric_value(u8"foobar")));
  CHECK_EQUAL(u8R"("foo\u1017bar")", to_string(metric_value(u8"foo\u1017bar")));

  CHECK_EQUAL(
      to_string(histogram::parse("[0..1=1]")),
      to_string(metric_value(histogram::parse("[0..1=1]"))));
}

TEST(to_ostream) {
  const auto to_ostream_ = [](const auto& v) {
    return (std::ostringstream() << v).str();
  };

  CHECK_EQUAL("(none)", to_ostream_(metric_value()));

  CHECK_EQUAL("false", to_ostream_(metric_value(false)));
  CHECK_EQUAL("true", to_ostream_(metric_value(true)));

  CHECK_EQUAL("0", to_ostream_(metric_value(0)));
  CHECK_EQUAL("17", to_ostream_(metric_value(17)));

  CHECK_EQUAL("-17", to_ostream_(metric_value(-17)));

  CHECK_EQUAL(to_ostream_(double(0.0)), to_ostream_(metric_value(0.0)));
  CHECK_EQUAL(to_ostream_(double(-0.0)), to_ostream_(metric_value(-0.0)));
  CHECK_EQUAL(to_ostream_(double(1.0)), to_ostream_(metric_value(1.0)));
  CHECK_EQUAL(to_ostream_(double(1e100)), to_ostream_(metric_value(1e100)));

  CHECK_EQUAL(u8R"("foobar")", to_ostream_(metric_value(u8"foobar")));
  CHECK_EQUAL(u8R"("foo\u1017bar")", to_ostream_(metric_value(u8"foo\u1017bar")));

  CHECK_EQUAL(
      to_ostream_(histogram::parse("[0..1=1]")),
      to_ostream_(metric_value(histogram::parse("[0..1=1]"))));
}

int main() {
  return UnitTest::RunAllTests();
}
