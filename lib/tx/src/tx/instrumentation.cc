#include <monsoon/tx/instrumentation.h>

namespace monsoon::tx {


instrumentation::group& instrumentation() {
  static auto impl = instrumentation::make_group("monsoon");
  return impl;
}


} /* namespace monsoon::tx */

namespace monsoon::tx::detail {


instrumentation::group& monsoon_tx_instrumentation() {
  static auto impl = instrumentation::make_group("tx", instrumentation());
  return impl;
}


} /* namespace monsoon::tx::detail */
