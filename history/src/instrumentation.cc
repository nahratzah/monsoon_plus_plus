#include <monsoon/history/instrumentation.h>
#include <monsoon/instrumentation.h>

namespace monsoon {


instrumentation::group& history_instrumentation() {
  static auto impl = instrumentation::make_group("history", monsoon_instrumentation());
  return impl;
}


} /* namespace monsoon */
