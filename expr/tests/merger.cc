#include <monsoon/expressions/merger.h>
#include <monsoon/expression.h>
#include <monsoon/match_clause.h>
#include <monsoon/tags.h>
#include <monsoon/objpipe/of.h>
#include <monsoon/objpipe/array.h>
#include <stdexcept>
#include "test_hacks.ii"
#include "UnitTest++/UnitTest++.h"

using namespace monsoon;
using monsoon::expressions::make_merger;

// Empty objpipe of scalar metrics.
auto empty_scalar_objpipe()
-> expression::scalar_objpipe {
  return objpipe::of<expression::scalar_objpipe::value_type>();
}

// Empty objpipe of tagged metrics.
auto empty_vector_objpipe()
-> expression::vector_objpipe {
  return objpipe::of<expression::vector_objpipe::value_type>();
}

// Create objpipe of scalar metrics.
auto scalar_objpipe(std::initializer_list<expression::scalar_emit_type> i)
-> expression::scalar_objpipe {
  return objpipe::new_array(i.begin(), i.end());
}

// Create objpipe of tagged metrics.
auto vector_objpipe(
    tags t,
    std::shared_ptr<const match_clause> mc,
    std::initializer_list<expression::scalar_emit_type> i)
-> expression::vector_objpipe {
  using vector_objpipe = expression::vector_objpipe;
  using vector_emit_type = expression::vector_emit_type;
  using factual_vector = expression::factual_vector;

  return scalar_objpipe(std::move(i))
      .transform(
          [t, mc](const expression::scalar_emit_type& s) {
            assert(s.data.index() == 0 || s.data.index() == 1);

            if (s.data.index() == 0)
              return vector_emit_type(s.tp, std::in_place_index<0>, t, std::get<0>(s.data));

            auto map = factual_vector(0, mc, mc);
            map[t] = std::get<1>(s.data);
            return vector_emit_type(s.tp, std::in_place_index<1>, std::move(map));
          });
}

// Predicate that filters away empty facts.
// Merger is allowed to optimize those away.
auto is_nonempty_fact(const expression::vector_emit_type& v) {
  return !std::get<1>(v.data).empty();
}

// Binary operator that fails its invocation.
auto dummy_binop(const metric_value& x, const metric_value& y)
-> metric_value {
  throw std::runtime_error("dummy_binop must not be invoked");
}

// Binary operator that emits value, if x == y.
auto same_binop(const metric_value& x, const metric_value& y)
-> metric_value {
  if (x == y) return x;
  throw std::runtime_error("same_binop: x == y -> false");
}

const tags test_tags = {{ "x", metric_value(5) }};
const std::shared_ptr<const match_clause> in_mc =
    std::make_shared<default_match_clause>();
const std::shared_ptr<const match_clause> out_mc =
    std::make_shared<default_match_clause>();

TEST(scalar_scalar_empty) {
  // Empty pipe
  CHECK_EQUAL(
      empty_scalar_objpipe()
          .to_vector(),
      make_merger(
          &dummy_binop,
          std::make_shared<default_match_clause>(),
          std::make_shared<default_match_clause>(),
          time_point::duration(5000),
          empty_scalar_objpipe(),
          empty_scalar_objpipe())
          .to_vector());
}

TEST(scalar_scalar_exact_tp_facts) {
  CHECK_EQUAL(
      scalar_objpipe({{ time_point(1000), std::in_place_index<1>, 17 }})
          .to_vector(),
      make_merger(
          &same_binop,
          std::make_shared<default_match_clause>(),
          std::make_shared<default_match_clause>(),
          time_point::duration(5000),
          scalar_objpipe({{ time_point(1000), std::in_place_index<1>, 17 }}),
          scalar_objpipe({{ time_point(1000), std::in_place_index<1>, 17 }}))
          .to_vector());
}

TEST(scalar_scalar_interpolate_facts_lhs) {
  CHECK_EQUAL(
      scalar_objpipe({{ time_point(2000), std::in_place_index<1>, 17 }})
          .to_vector(),
      make_merger(
          &same_binop,
          std::make_shared<default_match_clause>(),
          std::make_shared<default_match_clause>(),
          time_point::duration(5000),
          scalar_objpipe({
              { time_point(1000), std::in_place_index<1>, 16 },
              { time_point(3000), std::in_place_index<1>, 18 }
              }),
          scalar_objpipe({
              { time_point(2000), std::in_place_index<1>, 17 }
              }))
          .to_vector());
}

TEST(scalar_scalar_interpolate_facts_rhs) {
  CHECK_EQUAL(
      scalar_objpipe({{ time_point(2000), std::in_place_index<1>, 17 }})
          .to_vector(),
      make_merger(
          &same_binop,
          std::make_shared<default_match_clause>(),
          std::make_shared<default_match_clause>(),
          time_point::duration(5000),
          scalar_objpipe({
              { time_point(2000), std::in_place_index<1>, 17 }
              }),
          scalar_objpipe({
              { time_point(1000), std::in_place_index<1>, 16 },
              { time_point(3000), std::in_place_index<1>, 18 }
              }))
          .to_vector());
}

TEST(scalar_vector_empty) {
  // Empty pipe
  CHECK_EQUAL(
      empty_vector_objpipe()
          .to_vector(),
      make_merger(
          &dummy_binop,
          in_mc,
          out_mc,
          time_point::duration(5000),
          empty_scalar_objpipe(),
          empty_vector_objpipe())
          .to_vector());
}

TEST(scalar_vector_exact_tp_facts) {
  // Matching exact time points.
  CHECK_EQUAL(
      vector_objpipe(test_tags, out_mc, {{ time_point(1000), std::in_place_index<1>, 17 }})
          .to_vector(),
      make_merger(
          &same_binop,
          in_mc,
          out_mc,
          time_point::duration(5000),
          scalar_objpipe({{ time_point(1000), std::in_place_index<1>, 17 }}),
          vector_objpipe(test_tags, in_mc, {{ time_point(1000), std::in_place_index<1>, 17 }}))
          .to_vector());
}

TEST(scalar_vector_interpolate_facts_lhs) {
  // Check interpolation.
  CHECK_EQUAL(
      vector_objpipe(test_tags, out_mc, {{ time_point(2000), std::in_place_index<1>, 17 }})
          .to_vector(),
      make_merger(
          &same_binop,
          in_mc,
          out_mc,
          time_point::duration(5000),
          scalar_objpipe({
              { time_point(1000), std::in_place_index<1>, 16 },
              { time_point(3000), std::in_place_index<1>, 18 }
              }),
          vector_objpipe(test_tags, in_mc, {
              { time_point(2000), std::in_place_index<1>, 17 }
              }))
          .filter(&is_nonempty_fact)
          .to_vector());
}

TEST(scalar_vector_interpolate_facts_rhs) {
  // Check interpolation.
  CHECK_EQUAL(
      vector_objpipe(test_tags, out_mc, {{ time_point(2000), std::in_place_index<1>, 17 }})
          .to_vector(),
      make_merger(
          &same_binop,
          in_mc,
          out_mc,
          time_point::duration(5000),
          scalar_objpipe({
              { time_point(2000), std::in_place_index<1>, 17 }
              }),
          vector_objpipe(test_tags, in_mc, {
              { time_point(1000), std::in_place_index<1>, 16 },
              { time_point(3000), std::in_place_index<1>, 18 }
              }))
          .filter(&is_nonempty_fact)
          .to_vector());
}

TEST(vector_scalar_empty) {
  // Empty pipe
  CHECK_EQUAL(
      empty_vector_objpipe()
          .to_vector(),
      make_merger(
          &dummy_binop,
          in_mc,
          out_mc,
          time_point::duration(5000),
          empty_vector_objpipe(),
          empty_scalar_objpipe())
          .to_vector());
}

TEST(vector_scalar_exact_tp_facts) {
  // Matching exact time points.
  CHECK_EQUAL(
      vector_objpipe(test_tags, out_mc, {{ time_point(1000), std::in_place_index<1>, 17 }})
          .to_vector(),
      make_merger(
          &same_binop,
          in_mc,
          out_mc,
          time_point::duration(5000),
          vector_objpipe(test_tags, in_mc, {{ time_point(1000), std::in_place_index<1>, 17 }}),
          scalar_objpipe({{ time_point(1000), std::in_place_index<1>, 17 }}))
          .to_vector());
}

TEST(vector_scalar_interpolate_facts_lhs) {
  // Matching exact time points.
  CHECK_EQUAL(
      vector_objpipe(test_tags, out_mc, {{ time_point(2000), std::in_place_index<1>, 17 }})
          .to_vector(),
      make_merger(
          &same_binop,
          in_mc,
          out_mc,
          time_point::duration(5000),
          vector_objpipe(test_tags, in_mc, {
              { time_point(1000), std::in_place_index<1>, 16 },
              { time_point(3000), std::in_place_index<1>, 18 }
              }),
          scalar_objpipe({
              { time_point(2000), std::in_place_index<1>, 17 }
              }))
          .filter(&is_nonempty_fact)
          .to_vector());
}

TEST(vector_scalar_interpolate_facts_rhs) {
  // Matching exact time points.
  CHECK_EQUAL(
      vector_objpipe(test_tags, out_mc, {{ time_point(2000), std::in_place_index<1>, 17 }})
          .to_vector(),
      make_merger(
          &same_binop,
          in_mc,
          out_mc,
          time_point::duration(5000),
          vector_objpipe(test_tags, in_mc, {
              { time_point(2000), std::in_place_index<1>, 17 }
              }),
          scalar_objpipe({
              { time_point(1000), std::in_place_index<1>, 16 },
              { time_point(3000), std::in_place_index<1>, 18 }
              }))
          .filter(&is_nonempty_fact)
          .to_vector());
}

TEST(vector_vector_empty) {
  // Empty pipe
  CHECK_EQUAL(
      empty_vector_objpipe()
          .to_vector(),
      make_merger(
          &dummy_binop,
          in_mc,
          out_mc,
          time_point::duration(5000),
          empty_vector_objpipe(),
          empty_vector_objpipe())
          .to_vector());
}

TEST(vector_vector_exact_tp_facts) {
  // Matching exact time points.
  CHECK_EQUAL(
      vector_objpipe(test_tags, out_mc, {{ time_point(1000), std::in_place_index<1>, 17 }})
          .to_vector(),
      make_merger(
          &same_binop,
          in_mc,
          out_mc,
          time_point::duration(5000),
          vector_objpipe(test_tags, in_mc, {{ time_point(1000), std::in_place_index<1>, 17 }}),
          vector_objpipe(test_tags, in_mc, {{ time_point(1000), std::in_place_index<1>, 17 }}))
          .to_vector());
}

TEST(vector_vector_interpolate_facts_lhs) {
  // Matching exact time points.
  CHECK_EQUAL(
      vector_objpipe(test_tags, out_mc, {{ time_point(2000), std::in_place_index<1>, 17 }})
          .to_vector(),
      make_merger(
          &same_binop,
          in_mc,
          out_mc,
          time_point::duration(5000),
          vector_objpipe(test_tags, in_mc, {
              { time_point(1000), std::in_place_index<1>, 16 },
              { time_point(3000), std::in_place_index<1>, 18 }
              }),
          vector_objpipe(test_tags, in_mc, {
              { time_point(2000), std::in_place_index<1>, 17 }
              }))
          .filter(&is_nonempty_fact)
          .to_vector());
}

TEST(vector_vector_interpolate_facts_rhs) {
  // Matching exact time points.
  CHECK_EQUAL(
      vector_objpipe(test_tags, out_mc, {{ time_point(2000), std::in_place_index<1>, 17 }})
          .to_vector(),
      make_merger(
          &same_binop,
          in_mc,
          out_mc,
          time_point::duration(5000),
          vector_objpipe(test_tags, in_mc, {
              { time_point(2000), std::in_place_index<1>, 17 }
              }),
          vector_objpipe(test_tags, in_mc, {
              { time_point(1000), std::in_place_index<1>, 16 },
              { time_point(3000), std::in_place_index<1>, 18 }
              }))
          .filter(&is_nonempty_fact)
          .to_vector());
}

int main() {
  return UnitTest::RunAllTests();
};
