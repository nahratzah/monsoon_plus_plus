#include <monsoon/path_matcher.h>
#include <monsoon/simple_group.h>
#include <monsoon/metric_name.h>
#include "hacks.h"
#include "UnitTest++/UnitTest++.h"

using namespace monsoon;
using namespace std::string_view_literals;
using namespace std::string_literals;

TEST(config_string) {
  CHECK_EQUAL(
      "",
      to_string(path_matcher()));

  CHECK_EQUAL(
      "foo.bar.baz",
      to_string(path_matcher()
          .push_back_literal("foo")
          .push_back_literal("bar"s)
          .push_back_literal("baz"sv)));

  CHECK_EQUAL(
      R"('\U0001fe00'.'\b'.'\v')",
      to_string(path_matcher()
          .push_back_literal(u8"\U0001fe00")
          .push_back_literal("\b")
          .push_back_literal("\v")));

  CHECK_EQUAL(
      "*.foo.*",
      to_string(path_matcher()
          .push_back_wildcard()
          .push_back_literal("foo")
          .push_back_wildcard()));

  CHECK_EQUAL(
      "**.foo.*",
      to_string(path_matcher()
          .push_back_double_wildcard()
          .push_back_literal("foo")
          .push_back_wildcard()));
}

TEST(predicate_on_simple_group) {
  CHECK_EQUAL(
      true,
      path_matcher()
          .push_back_literal("foo")
          .push_back_literal("bar")
          (simple_group({ "foo", "bar" })));
  CHECK_EQUAL(
      false,
      path_matcher()
          .push_back_literal("foo")
          .push_back_literal("bar")
          (simple_group({ "foo" })));
  CHECK_EQUAL(
      false,
      path_matcher()
          .push_back_literal("foo")
          .push_back_literal("bar")
          (simple_group({ "foo", "barium" })));

  CHECK_EQUAL(
      true,
      path_matcher()
          .push_back_literal("foo")
          .push_back_wildcard()
          (simple_group({ "foo", "barium" })));
  CHECK_EQUAL(
      false,
      path_matcher()
          .push_back_literal("foo")
          .push_back_wildcard()
          (simple_group({ "foo" })));

  CHECK_EQUAL(
      true,
      path_matcher()
          .push_back_literal("foo")
          .push_back_double_wildcard()
          (simple_group({ "foo", "barium" })));
  CHECK_EQUAL(
      true,
      path_matcher()
          .push_back_literal("foo")
          .push_back_double_wildcard()
          (simple_group({ "foo" })));
  CHECK_EQUAL(
      true,
      path_matcher()
          .push_back_literal("foo")
          .push_back_double_wildcard()
          (simple_group({ "foo", "bar", "baz" })));
  CHECK_EQUAL(
      true,
      path_matcher()
          .push_back_literal("foo")
          .push_back_double_wildcard()
          .push_back_literal("baz")
          (simple_group({ "foo", "bar", "baz" })));
  CHECK_EQUAL(
      true,
      path_matcher()
          .push_back_literal("foo")
          .push_back_double_wildcard()
          .push_back_literal("bar")
          .push_back_literal("baz")
          (simple_group({ "foo", "bar", "baz" })));
}

TEST(predicate_on_metric_name) {
  CHECK_EQUAL(
      true,
      path_matcher()
          .push_back_literal("foo")
          .push_back_literal("bar")
          (metric_name({ "foo", "bar" })));
  CHECK_EQUAL(
      false,
      path_matcher()
          .push_back_literal("foo")
          .push_back_literal("bar")
          (metric_name({ "foo" })));
  CHECK_EQUAL(
      false,
      path_matcher()
          .push_back_literal("foo")
          .push_back_literal("bar")
          (metric_name({ "foo", "barium" })));

  CHECK_EQUAL(
      true,
      path_matcher()
          .push_back_literal("foo")
          .push_back_wildcard()
          (metric_name({ "foo", "barium" })));
  CHECK_EQUAL(
      false,
      path_matcher()
          .push_back_literal("foo")
          .push_back_wildcard()
          (metric_name({ "foo" })));

  CHECK_EQUAL(
      true,
      path_matcher()
          .push_back_literal("foo")
          .push_back_double_wildcard()
          (metric_name({ "foo", "barium" })));
  CHECK_EQUAL(
      true,
      path_matcher()
          .push_back_literal("foo")
          .push_back_double_wildcard()
          (metric_name({ "foo" })));
  CHECK_EQUAL(
      true,
      path_matcher()
          .push_back_literal("foo")
          .push_back_double_wildcard()
          (metric_name({ "foo", "bar", "baz" })));
  CHECK_EQUAL(
      true,
      path_matcher()
          .push_back_literal("foo")
          .push_back_double_wildcard()
          .push_back_literal("baz")
          (metric_name({ "foo", "bar", "baz" })));
  CHECK_EQUAL(
      true,
      path_matcher()
          .push_back_literal("foo")
          .push_back_double_wildcard()
          .push_back_literal("bar")
          .push_back_literal("baz")
          (metric_name({ "foo", "bar", "baz" })));
  CHECK_EQUAL(
      true,
      path_matcher()
          .push_back_literal("foo")
          .push_back_double_wildcard()
          .push_back_wildcard()
          .push_back_literal("baz")
          (metric_name({ "foo", "bar", "baz" })));
  CHECK_EQUAL(
      false,
      path_matcher()
          .push_back_literal("foo")
          .push_back_double_wildcard()
          .push_back_wildcard()
          .push_back_literal("baz")
          (metric_name({ "foo", "baz" })));
}

TEST(overlap) {
  CHECK_EQUAL(
      true,
      has_overlap(
          path_matcher()
              .push_back_literal("foo")
              .push_back_literal("bar"),
          path_matcher()
              .push_back_literal("foo")
              .push_back_literal("bar")));

  CHECK_EQUAL(
      false,
      has_overlap(
          path_matcher()
              .push_back_literal("foo")
              .push_back_literal("bar"),
          path_matcher()
              .push_back_literal("foo")
              .push_back_literal("xxx")));

  CHECK_EQUAL(
      true,
      has_overlap(
          path_matcher()
              .push_back_literal("foo")
              .push_back_literal("bar"),
          path_matcher()
              .push_back_literal("foo")
              .push_back_wildcard()));

  CHECK_EQUAL(
      true,
      has_overlap(
          path_matcher()
              .push_back_literal("foo")
              .push_back_wildcard(),
          path_matcher()
              .push_back_literal("foo")
              .push_back_literal("bar")));

  CHECK_EQUAL(
      true,
      has_overlap(
          path_matcher()
              .push_back_literal("foo")
              .push_back_literal("bar"),
          path_matcher()
              .push_back_literal("foo")
              .push_back_double_wildcard()
              .push_back_literal("bar")));

  CHECK_EQUAL(
      true,
      has_overlap(
          path_matcher()
              .push_back_literal("foo")
              .push_back_double_wildcard()
              .push_back_literal("bar"),
          path_matcher()
              .push_back_literal("foo")
              .push_back_literal("bar")));

  CHECK_EQUAL(
      true,
      has_overlap(
          path_matcher()
              .push_back_wildcard()
              .push_back_double_wildcard()
              .push_back_wildcard(),
          path_matcher()
              .push_back_literal("foo")
              .push_back_literal("bar")));

  CHECK_EQUAL(
      false,
      has_overlap(
          path_matcher()
              .push_back_wildcard()
              .push_back_double_wildcard()
              .push_back_wildcard(),
          path_matcher()
              .push_back_literal("bar")));

  CHECK_EQUAL(
      false,
      has_overlap(
          path_matcher()
              .push_back_literal("bar"),
          path_matcher()
              .push_back_wildcard()
              .push_back_double_wildcard()
              .push_back_wildcard()));
}

int main() {
  return UnitTest::RunAllTests();
}
