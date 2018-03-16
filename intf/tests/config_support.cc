#include <monsoon/config_support.h>
#include <string_view>
#include "UnitTest++/UnitTest++.h"

using namespace monsoon;
using namespace std::string_view_literals;

TEST(quote_str) {
  CHECK_EQUAL(
      R"("")"sv,
      quoted_string(""));

  CHECK_EQUAL(
      R"("\a\b\v\f\r\0\n")"sv,
      quoted_string("\a\b\v\f\r\0\n"sv));

  CHECK_EQUAL(
      R"("\u10ff")"sv,
      quoted_string("\u10ff"));

  CHECK_EQUAL(
      R"("\\foobar")"sv,
      quoted_string("\\foobar"));

  CHECK_EQUAL(
      R"("foo\"bar")"sv,
      quoted_string("foo\"bar"));

  CHECK_EQUAL(
      R"("foo'bar")"sv,
      quoted_string("foo'bar"));

  CHECK_EQUAL(
      R"("\U00100010")"sv,
      quoted_string("\U00100010"));
}

TEST(quote_ident) {
  CHECK_EQUAL(
      R"('')"sv,
      maybe_quote_identifier(""));

  CHECK_EQUAL(
      R"(foobar)"sv,
      maybe_quote_identifier("foobar"));

  CHECK_EQUAL(
      R"(_X)"sv,
      maybe_quote_identifier("_X"));

  CHECK_EQUAL(
      R"(x09)"sv,
      maybe_quote_identifier("x09"));

  CHECK_EQUAL(
      R"('9')"sv,
      maybe_quote_identifier("9"));

  CHECK_EQUAL(
      R"('\a\b\\\n\0')"sv,
      maybe_quote_identifier("\a\b\\\n\0"sv));

  CHECK_EQUAL(
      R"('\'"')"sv,
      maybe_quote_identifier("\'\""));

  CHECK_EQUAL(
      R"('\u10ff')"sv,
      maybe_quote_identifier("\u10ff"));

  CHECK_EQUAL(
      R"('\U00100010')"sv,
      maybe_quote_identifier("\U00100010"));
}

int main() {
  return UnitTest::RunAllTests();
}
