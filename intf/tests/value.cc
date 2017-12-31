#include <monsoon/metric_value.h>
#include <monsoon/histogram.h>
#include <string_view>
#include <utility>
#include <tuple>
#include "UnitTest++/UnitTest++.h"

using namespace monsoon;
using namespace std::string_view_literals;

TEST(parse) {
  CHECK_EQUAL(metric_value("foobar"), metric_value::parse(R"("foobar")"));
  CHECK_EQUAL(metric_value("foo\nbar"), metric_value::parse(R"("foo\nbar")"));
  CHECK_EQUAL(metric_value("foo\tbar"), metric_value::parse(R"("foo\tbar")"));
  CHECK_EQUAL(metric_value("foo\0bar"sv), metric_value::parse(R"("foo\0bar")"));
  CHECK_EQUAL(metric_value("foo\7bar"), metric_value::parse(R"("foo\7bar")"));
  CHECK_EQUAL(metric_value("foo\17bar"), metric_value::parse(R"("foo\17bar")"));
  CHECK_EQUAL(metric_value("foo\017bar"), metric_value::parse(R"("foo\017bar")"));
  CHECK_EQUAL(metric_value("foo\x17""bar"), metric_value::parse(R"("foo\x17bar")"));
  CHECK_EQUAL(metric_value("foo\u1017""bar"), metric_value::parse(R"("foo\u1017bar")"));
  CHECK_EQUAL(metric_value("foo\U00011017""bar"), metric_value::parse(R"("foo\U00011017bar")"));

  CHECK_EQUAL(metric_value(42), metric_value::parse(R"(42)"));
  CHECK_EQUAL(metric_value(-0), metric_value::parse(R"(-0)"));
  CHECK_EQUAL(metric_value(0.0), metric_value::parse(R"(0.0)"));
  CHECK_EQUAL(metric_value(-0.0), metric_value::parse(R"(-0.0)"));
  CHECK_EQUAL(metric_value(1e4), metric_value::parse(R"(1e4)"));
  CHECK_EQUAL(metric_value(-1e4), metric_value::parse(R"(-1e4)"));

  CHECK_EQUAL(metric_value(true), metric_value::parse(R"(true)"));
  CHECK_EQUAL(metric_value(false), metric_value::parse(R"(false)"));

  CHECK_EQUAL(metric_value(histogram()), metric_value::parse(R"([])"));
  CHECK_EQUAL(
      metric_value(histogram({ std::make_tuple(histogram::range(0.0, 1.0), 1.0)})),
      metric_value::parse(R"([0.0..1.0=1.0])"));
  CHECK_EQUAL(
      metric_value(histogram({ std::make_tuple(histogram::range(0.0, 1.0), 1.0)})),
      metric_value::parse(R"([0.0..1=1])"));
  CHECK_EQUAL(
      metric_value(histogram({ std::make_tuple(histogram::range(0.0, 0.1), 1.0)})),
      metric_value::parse(R"([0...1=1])"));
  CHECK_EQUAL(
      metric_value(histogram({ std::make_tuple(histogram::range(0.0, 1.0), 1.0)})),
      metric_value::parse(R"([0..1=1])"));
  CHECK_EQUAL(
      metric_value(histogram({
              std::make_tuple(histogram::range(0.0, 1.0), 1.0),
              std::make_tuple(histogram::range(3.0, 4.0), 5.0)
              })),
      metric_value::parse(R"([ 0 .. 1 = 1, 3 .. 4 = 5])"));
}

int main() {
  return UnitTest::RunAllTests();
}
