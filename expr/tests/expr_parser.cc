#include <monsoon/metric_source.h>
#include <monsoon/expression.h>
#include <objpipe/of.h>
#include "UnitTest++/UnitTest++.h"
#include <vector>
#include <optional>
#include <tuple>
#include <stdexcept>
#include "test_hacks.ii"

using namespace monsoon;

class mock_metric_source_for_emit_time
: public metric_source
{
 public:
  virtual auto emit(
      time_range tr,
      path_matcher group_filter,
      tag_matcher tag_filter,
      path_matcher metric_filter,
      time_point::duration slack = time_point::duration(0)) const
  -> objpipe::reader<emit_type> {
    throw std::runtime_error("unimplemented mock");
  }

  auto emit_time(
      time_range tr,
      time_point::duration slack) const
  -> objpipe::reader<time_point> {
    last_emit_time.emplace(std::move(tr), std::move(slack));
    return objpipe::of(result_emit_time.value())
        .iterate();
  }

  std::optional<std::vector<time_point>> result_emit_time;
  mutable std::optional<std::tuple<time_range, time_point::duration>>
      last_emit_time;
};

TEST(constant) {
  mock_metric_source_for_emit_time mms;
  mms.result_emit_time.emplace({
      time_point(10000),
      time_point(20000),
      time_point(30000)
  });

  auto expr_ptr = expression::parse("(1 << 2) * 10 + 1 * ----2");

  CHECK_EQUAL(true, expr_ptr->is_scalar());
  CHECK_EQUAL(false, expr_ptr->is_vector());
  CHECK_EQUAL("(1 << 2) * 10 + 1 * ----2", to_string(*expr_ptr));

  auto reader_variant =
      (*expr_ptr)(mms, time_range(), time_point::duration(10 * 1000));
  CHECK_EQUAL(
      std::make_tuple(time_range(), time_point::duration(10 * 1000)),
      mms.last_emit_time.value());

  // Created queue is independant of lifetime of expression.
  expr_ptr.reset();

  REQUIRE CHECK_EQUAL(0u, reader_variant.index());
  auto reader = std::get<0>(std::move(reader_variant));

  for (time_point tp : mms.result_emit_time.value()) {
    CHECK_EQUAL(
        expression::scalar_emit_type(
            tp,
            std::in_place_index<1>,
            metric_value(42)),
        reader.pull());
  }
  CHECK_EQUAL(true, reader.empty());
}

int main() {
  return UnitTest::RunAllTests();
};
