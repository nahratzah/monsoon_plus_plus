#include <monsoon/metric_source.h>
#include <monsoon/expressions/constant.h>
#include "UnitTest++/UnitTest++.h"

#if 0
class mock_metric_source
: public monsoon::metric_source
{
 public:
  auto emit_time(
      monsoon::time_range
      monsoon::time_point::duration) const
  -> monsoon::objpipe::reader<time_point> {
    return monsoon::objpipe::collection(tps);
  }
};
#endif

int main() {
  return UnitTest::RunAllTests();
};
