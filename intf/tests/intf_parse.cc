#include <monsoon/metric_value.h>
#include <monsoon/metric_name.h>
#include <monsoon/simple_group.h>
#include <monsoon/tags.h>
#include <monsoon/group_name.h>
#include <monsoon/histogram.h>
#include <string_view>
#include <utility>
#include <tuple>
#include "UnitTest++/UnitTest++.h"

using namespace monsoon;
using namespace std::string_view_literals;

TEST(metric_value) {
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
  CHECK_EQUAL(metric_value(-42), metric_value::parse(R"(-42)"));
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

TEST(metric_name) {
  CHECK_EQUAL(metric_name({"foo", "bar"}), metric_name::parse("foo.bar"));
  CHECK_EQUAL(metric_name({"foo", "bar"}), metric_name::parse("'foo'.bar"));
  CHECK_EQUAL(metric_name({"foo", "bar"}), metric_name::parse("foo.'bar'"));
  CHECK_EQUAL(metric_name({"foo.bar"}), metric_name::parse("'foo.bar'"));
}

TEST(simple_group) {
  CHECK_EQUAL(simple_group({"foo", "bar"}), simple_group::parse("foo.bar"));
  CHECK_EQUAL(simple_group({"foo", "bar"}), simple_group::parse("'foo'.bar"));
  CHECK_EQUAL(simple_group({"foo", "bar"}), simple_group::parse("foo.'bar'"));
  CHECK_EQUAL(simple_group({"foo.bar"}), simple_group::parse("'foo.bar'"));
  CHECK_EQUAL(simple_group({"foo\bbar"}), simple_group::parse(R"('foo\bbar')"));
  CHECK_EQUAL(simple_group({"foo\x22""bar"}), simple_group::parse(R"('foo\x22bar')"));
  CHECK_EQUAL(simple_group({u8"foo\u1022bar"}), simple_group::parse(R"('foo\u1022bar')"));
  CHECK_EQUAL(simple_group({u8"foo\U00101022bar"}), simple_group::parse(R"('foo\U00101022bar')"));
  CHECK_EQUAL(simple_group({u8"foo\022bar"}), simple_group::parse(R"('foo\022bar')"));
}

TEST(tags) {
  CHECK_EQUAL(
      tags(),
      tags::parse("{}"));
  CHECK_EQUAL(
      tags({{"foo", metric_value(42)}}),
      tags::parse("{foo=42}"));
  CHECK_EQUAL(
      tags({{u8"foo\U0001fffe", metric_value(42)}}),
      tags::parse(R"({'foo\U0001fffe'=42})"));
  CHECK_EQUAL(
      tags({{"foo", metric_value(42)}}),
      tags::parse("{'foo'=42}"));
  CHECK_EQUAL(
      tags({{"foo", metric_value("42")}}),
      tags::parse(R"({'foo'="42"})"));
  CHECK_EQUAL(
      tags({{"foo", metric_value("42")}, {"bar", metric_value(false)}}),
      tags::parse(R"({'foo'="42", bar=false})"));
}

TEST(group_name) {
  CHECK_EQUAL(
      group_name({"foo", "bar"}),
      group_name::parse(R"(foo.bar)"));
  CHECK_EQUAL(
      group_name({"foo.bar"}),
      group_name::parse(R"('foo.bar')"));
  CHECK_EQUAL(
      group_name({"foo.bar"}),
      group_name::parse(R"('foo.bar' { })"));
  CHECK_EQUAL(
      group_name({"foo", "bar"}, tags({{"pi", metric_value(3.14)}})),
      group_name::parse(R"(foo.bar { pi=3.14 })"));
}

int main() {
  return UnitTest::RunAllTests();
}
