#include <monsoon/simple_group.h>
#include <monsoon/objpipe/of.h>
#include "hacks.h"
#include "UnitTest++/UnitTest++.h"

using namespace monsoon;
using namespace std::string_view_literals;
using namespace std::string_literals;

TEST(constructor) {
  CHECK_EQUAL(
      simple_group::path_type(),
      simple_group().get_path());

  CHECK_EQUAL(
      simple_group::path_type({ simple_group::path_type::value_type("foo"), simple_group::path_type::value_type("bar") }),
      simple_group({ "foo", "bar" }).get_path());

  CHECK_EQUAL(
      simple_group::path_type({ simple_group::path_type::value_type("foo"), simple_group::path_type::value_type("bar") }),
      simple_group({ "foo"s, "bar"s }).get_path());

  std::vector<std::string> sv = { "foo", "bar", "baz" };
  CHECK_EQUAL(
      simple_group::path_type({
          simple_group::path_type::value_type("foo"),
          simple_group::path_type::value_type("bar"),
          simple_group::path_type::value_type("baz")
      }),
      simple_group(sv.begin(), sv.end()).get_path());

  auto input_iter_list = objpipe::of("foo"sv, "bar"sv, "baz"sv);
  CHECK_EQUAL(
      simple_group::path_type({
          simple_group::path_type::value_type("foo"),
          simple_group::path_type::value_type("bar"),
          simple_group::path_type::value_type("baz")
      }),
      simple_group(sv.begin(), sv.end()).get_path());
}

TEST(config_string) {
  CHECK_EQUAL(
      "",
      simple_group().config_string());

  CHECK_EQUAL(
      "foo.bar.baz",
      simple_group({ "foo", "bar", "baz" }).config_string());

  CHECK_EQUAL(
      R"('\U0001fe00'.'\b'.'\v')",
      simple_group({ u8"\U0001fe00", "\b", "\v" }).config_string());
}

TEST(equality) {
  CHECK_EQUAL(
      true,
      simple_group({ "foo", "bar" }) == simple_group({ "foo"s, "bar"s }));
  CHECK_EQUAL(
      false,
      simple_group({ "foo", "bar" }) != simple_group({ "foo"s, "bar"s }));

  CHECK_EQUAL(
      false,
      simple_group({ "foo", "bar" }) == simple_group({ "bar"s }));
  CHECK_EQUAL(
      true,
      simple_group({ "foo", "bar" }) != simple_group({ "bar"s }));

  CHECK_EQUAL(
      true,
      simple_group({ "foo" }) < simple_group({ "foo", "b" }));
  CHECK_EQUAL(
      false,
      simple_group({ "foo" }) < simple_group({ "foo" }));
  CHECK_EQUAL(
      false,
      simple_group({ "foo", "b" }) < simple_group({ "foo" }));

  CHECK_EQUAL(
      true,
      simple_group({ "X" }) < simple_group({ "Y" }));
  CHECK_EQUAL(
      false,
      simple_group({ "Y" }) < simple_group({ "X" }));
}

int main() {
  return UnitTest::RunAllTests();
}
