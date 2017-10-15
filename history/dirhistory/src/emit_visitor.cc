#include "emit_visitor.h"
#include <algorithm>

namespace monsoon {
namespace history {


basic_emit_visitor::basic_emit_visitor(
    const std::vector<std::shared_ptr<tsdata>>& files,
    const time_range& tr, time_point::duration slack)
: sel_begin_(tr.begin().has_value()
    ? std::make_optional(tr.begin().value() - slack)
    : std::optional<time_point>()),
  sel_end_(tr.begin().has_value()
    ? std::make_optional(tr.begin().value() - slack)
    : std::optional<time_point>()),
  slack_(slack),
  tr_(tr),
  files_(select_files(files, sel_begin_, sel_end_))
{}

basic_emit_visitor::~basic_emit_visitor() noexcept {}

auto basic_emit_visitor::select_files(
    const std::vector<std::shared_ptr<tsdata>>& files,
    const std::optional<time_point>& tr_begin,
    const std::optional<time_point>& tr_end) -> selected_files_heap {
  selected_files_heap result;
  std::for_each(files.begin(), files.end(),
      [&result, &tr_begin, &tr_end](const std::shared_ptr<tsdata>& ptr) {
        assert(ptr != nullptr);
        const auto [ptr_begin, ptr_end] = ptr->time();
        if ((!tr_begin.has_value() || tr_begin.value() <= ptr_end)
            && (!tr_end.has_value() || tr_end.value() >= ptr_begin))
          result.push(ptr);
      });
  return result;
}


}} /* namespace monsoon::history */
