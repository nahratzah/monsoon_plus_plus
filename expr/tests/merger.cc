#include <monsoon/expressions/merger.h>
#include <monsoon/expression.h>
#include <monsoon/match_clause.h>
#include <monsoon/objpipe/of.h>
#include "test_hacks.ii"
#include "UnitTest++/UnitTest++.h"

using namespace monsoon;
using monsoon::expressions::make_merger;

auto dummy_scalar_objpipe()
-> expression::scalar_objpipe {
  return objpipe::of<expression::scalar_objpipe::value_type>();
}

auto dummy_vector_objpipe()
-> expression::vector_objpipe {
  return objpipe::of<expression::vector_objpipe::value_type>();
}

auto dummy_binop(const metric_value& x, const metric_value& y)
-> metric_value {
  return {};
}

TEST(scalar_scalar) {
  // Empty pipe
  CHECK_EQUAL(
      std::vector<expression::scalar_objpipe::value_type>(),
      make_merger(
          dummy_binop,
          std::make_shared<default_match_clause>(),
          std::make_shared<default_match_clause>(),
          time_point::duration(5000),
          dummy_scalar_objpipe(),
          dummy_scalar_objpipe())
          .to_vector());
}

TEST(scalar_vector) {
  // Empty pipe
  CHECK_EQUAL(
      std::vector<expression::vector_objpipe::value_type>(),
      make_merger(
          dummy_binop,
          std::make_shared<default_match_clause>(),
          std::make_shared<default_match_clause>(),
          time_point::duration(5000),
          dummy_scalar_objpipe(),
          dummy_vector_objpipe())
          .to_vector());
}

TEST(vector_scalar) {
  // Empty pipe
  CHECK_EQUAL(
      std::vector<expression::vector_objpipe::value_type>(),
      make_merger(
          dummy_binop,
          std::make_shared<default_match_clause>(),
          std::make_shared<default_match_clause>(),
          time_point::duration(5000),
          dummy_vector_objpipe(),
          dummy_scalar_objpipe())
          .to_vector());
}

TEST(vector_vector) {
  // Empty pipe
  CHECK_EQUAL(
      std::vector<expression::vector_objpipe::value_type>(),
      make_merger(
          dummy_binop,
          std::make_shared<default_match_clause>(),
          std::make_shared<default_match_clause>(),
          time_point::duration(5000),
          dummy_vector_objpipe(),
          dummy_vector_objpipe())
          .to_vector());
}

int main() {
  return UnitTest::RunAllTests();
};
