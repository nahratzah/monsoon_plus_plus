#include <monsoon/tags.h>
#include <string_view>
#include <string>
#include <vector>
#include <monsoon/objpipe/array.h>
#include "hacks.h"
#include "UnitTest++/UnitTest++.h"

using namespace monsoon;
using namespace std::string_view_literals;
using namespace std::string_literals;

TEST(constructor) {
  // Empty tags.
  CHECK_EQUAL(
      tags::map_type(),
      tags().get_map());

  // Initializer list.
  CHECK_EQUAL(
      tags::map_type({{"foo", metric_value("bar")}}),
      tags({{"foo", metric_value("bar")}}).get_map());

  // Vector.
  CHECK_EQUAL(
      tags::map_type({
          {"foo", metric_value("bar")},
          {"bar", metric_value(16)}
      }),
      tags(std::vector<std::tuple<std::string, metric_value>>({
              {"foo", metric_value("bar")},
              {"bar", metric_value(16)}
          })).get_map());

  // Any string->metric_value map.
  CHECK_EQUAL(
      tags::map_type({
          {"foo", metric_value("bar")},
          {"bar", metric_value(16)}
      }),
      tags(std::map<std::string, metric_value>({
              {"foo", metric_value("bar")},
              {"bar", metric_value(16)}
          })).get_map());
  CHECK_EQUAL(
      tags::map_type({
          {"foo", metric_value("bar")},
          {"bar", metric_value(16)}
      }),
      tags(std::unordered_map<std::string, metric_value>({
              {"foo", metric_value("bar")},
              {"bar", metric_value(16)}
          })).get_map());
  CHECK_EQUAL(
      tags::map_type({
          {"foo", metric_value("bar")},
          {"bar", metric_value(16)}
      }),
      tags(std::map<std::string_view, metric_value, std::less<>>({
              {"foo"sv, metric_value("bar")},
              {"bar"sv, metric_value(16)}
          })).get_map());

  // Iterators.
  std::vector<std::tuple<std::string, metric_value>> init = {
      {"foo", metric_value("bar")},
      {"bar", metric_value(16)}
  };
  CHECK_EQUAL(
      tags::map_type({
          {"foo", metric_value("bar")},
          {"bar", metric_value(16)}
      }),
      tags(init.begin(), init.end()).get_map());
  // Input iterator.
  auto init_input_iters = objpipe::new_array(init.begin(), init.end());
  CHECK_EQUAL(
      tags::map_type({
          {"foo", metric_value("bar")},
          {"bar", metric_value(16)}
      }),
      tags(init_input_iters.begin(), init_input_iters.end()).get_map());
}

TEST(to_string) {
  CHECK_EQUAL("{}", to_string(tags()));
  CHECK_EQUAL("{bar=7, foo=6}", to_string(tags({{"foo", metric_value(6)}, {"bar", metric_value(7)}})));

  // Proper name escaping.
  CHECK_EQUAL(
      R"({'\U00010010'=9})",
      to_string(tags({{u8"\U00010010", metric_value(9)}})));

  // Proper value escaping.
  CHECK_EQUAL(
      R"({foo="\U00010010"})",
      to_string(tags({{"foo", metric_value(u8"\U00010010")}})));
}

TEST(equality) {
  CHECK_EQUAL(
      true,
      tags() == tags());
  CHECK_EQUAL(
      false,
      tags() != tags());
  CHECK_EQUAL(
      false,
      tags() < tags());
  CHECK_EQUAL(
      false,
      tags() > tags());
  CHECK_EQUAL(
      true,
      tags() <= tags());
  CHECK_EQUAL(
      true,
      tags() >= tags());

  CHECK_EQUAL(
      false,
      tags({{"foo", metric_value(1)}}) == tags({{"foo", metric_value(2)}}));
  CHECK_EQUAL(
      true,
      tags({{"foo", metric_value(1)}}) != tags({{"foo", metric_value(2)}}));
  CHECK_EQUAL(
      true,
      tags({{"foo", metric_value(1)}}) < tags({{"foo", metric_value(2)}}));
  CHECK_EQUAL(
      false,
      tags({{"foo", metric_value(1)}}) > tags({{"foo", metric_value(2)}}));
  CHECK_EQUAL(
      true,
      tags({{"foo", metric_value(1)}}) <= tags({{"foo", metric_value(2)}}));
  CHECK_EQUAL(
      false,
      tags({{"foo", metric_value(1)}}) >= tags({{"foo", metric_value(2)}}));

  CHECK_EQUAL(
      false,
      tags({{"bar", metric_value(1)}}) == tags({{"foo", metric_value(1)}}));
  CHECK_EQUAL(
      true,
      tags({{"bar", metric_value(1)}}) != tags({{"foo", metric_value(1)}}));
  CHECK_EQUAL(
      true,
      tags({{"bar", metric_value(1)}}) < tags({{"foo", metric_value(1)}}));
  CHECK_EQUAL(
      false,
      tags({{"bar", metric_value(1)}}) > tags({{"foo", metric_value(1)}}));
  CHECK_EQUAL(
      true,
      tags({{"bar", metric_value(1)}}) <= tags({{"foo", metric_value(1)}}));
  CHECK_EQUAL(
      false,
      tags({{"bar", metric_value(1)}}) >= tags({{"foo", metric_value(1)}}));

  // Lexicographical comparison
  const tags bar = {{"bar", metric_value(1)}};
  const tags foo_bar = {{"foo", metric_value(1)}, {"bar", metric_value(1)}};
  CHECK_EQUAL(false, bar == foo_bar);
  CHECK_EQUAL(true,  bar != foo_bar);
  CHECK_EQUAL(true,  bar <  foo_bar);
  CHECK_EQUAL(false, bar >  foo_bar);
  CHECK_EQUAL(true,  bar <= foo_bar);
  CHECK_EQUAL(false, bar >= foo_bar);
}

int main() {
  return UnitTest::RunAllTests();
}