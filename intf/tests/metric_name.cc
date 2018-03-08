#include <monsoon/metric_name.h>
#include <monsoon/simple_group.h>
#include <monsoon/objpipe/of.h>
#include "hacks.h"
#include "UnitTest++/UnitTest++.h"

using namespace monsoon;
using namespace std::string_view_literals;
using namespace std::string_literals;

static_assert(!std::is_constructible_v<metric_name, simple_group>,
    "Implicit conversion between simple_group and metric_name is not allowed: it would cause confusion");

TEST(constructor) {
  CHECK_EQUAL(
      metric_name::path_type(),
      metric_name().get_path());

  CHECK_EQUAL(
      metric_name::path_type({ metric_name::path_type::value_type("foo"), metric_name::path_type::value_type("bar") }),
      metric_name({ "foo", "bar" }).get_path());

  CHECK_EQUAL(
      metric_name::path_type({ metric_name::path_type::value_type("foo"), metric_name::path_type::value_type("bar") }),
      metric_name({ "foo"s, "bar"s }).get_path());

  std::vector<std::string> sv = { "foo", "bar", "baz" };
  CHECK_EQUAL(
      metric_name::path_type({
          metric_name::path_type::value_type("foo"),
          metric_name::path_type::value_type("bar"),
          metric_name::path_type::value_type("baz")
      }),
      metric_name(sv.begin(), sv.end()).get_path());

  auto input_iter_list = objpipe::of("foo"sv, "bar"sv, "baz"sv);
  CHECK_EQUAL(
      metric_name::path_type({
          metric_name::path_type::value_type("foo"),
          metric_name::path_type::value_type("bar"),
          metric_name::path_type::value_type("baz")
      }),
      metric_name(sv.begin(), sv.end()).get_path());
}

TEST(config_string) {
  CHECK_EQUAL(
      "",
      metric_name().config_string());

  CHECK_EQUAL(
      "foo.bar.baz",
      metric_name({ "foo", "bar", "baz" }).config_string());

  CHECK_EQUAL(
      R"('\U0001fe00'.'\b'.'\v')",
      metric_name({ u8"\U0001fe00", "\b", "\v" }).config_string());
}

TEST(equality) {
  CHECK_EQUAL(
      true,
      metric_name({ "foo", "bar" }) == metric_name({ "foo"s, "bar"s }));
  CHECK_EQUAL(
      false,
      metric_name({ "foo", "bar" }) != metric_name({ "foo"s, "bar"s }));

  CHECK_EQUAL(
      false,
      metric_name({ "foo", "bar" }) == metric_name({ "bar"s }));
  CHECK_EQUAL(
      true,
      metric_name({ "foo", "bar" }) != metric_name({ "bar"s }));

  CHECK_EQUAL(
      true,
      metric_name({ "foo" }) < metric_name({ "foo", "b" }));
  CHECK_EQUAL(
      false,
      metric_name({ "foo" }) < metric_name({ "foo" }));
  CHECK_EQUAL(
      false,
      metric_name({ "foo", "b" }) < metric_name({ "foo" }));

  CHECK_EQUAL(
      true,
      metric_name({ "X" }) < metric_name({ "Y" }));
  CHECK_EQUAL(
      false,
      metric_name({ "Y" }) < metric_name({ "X" }));
}

int main() {
  return UnitTest::RunAllTests();
}
