#include <monsoon/history/instrumentation.h>
#include <monsoon/instrumentation.h>

namespace monsoon {


instrumentation::group&& history_instrumentation = instrumentation::make_group("history", monsoon_instrumentation);


} /* namespace monsoon */
