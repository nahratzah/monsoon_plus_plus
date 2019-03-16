#include <monsoon/history/collect_history.h>
#include <objpipe/of.h>

namespace monsoon {


collect_history::~collect_history() noexcept = default;

auto collect_history::push_back(const metric_emit& m) -> void {
  if (!std::get<1>(m).empty()) do_push_back_(m);
}


} /* namespace monsoon */
