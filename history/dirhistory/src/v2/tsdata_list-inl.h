#ifndef V2_TSDATA_LIST_INL_H
#define V2_TSDATA_LIST_INL_H

#include <stack>

namespace monsoon {
namespace history {
namespace v2 {


template<typename Fn>
void tsdata_v2_list::visit(Fn fn) const {
  if (data_.file_ptr().offset() == 0u) // Empty file, bail out early
    return;

  // Build stack of tsdata_list entries.
  std::stack<std::shared_ptr<const tsdata_list>> data_stack;
  data_stack.push(data_.get());
  const auto dict_ptr = data_stack.top()->dictionary();
  for (auto pred_ptr = data_stack.top()->pred();
      pred_ptr != nullptr;
      pred_ptr = data_stack.top()->pred()) {
    data_stack.push(pred_ptr);
  }

  // Process stack of tsdata_list entries.
  while (!data_stack.empty()) {
    const auto dptr = data_stack.top();
    data_stack.pop();
    std::invoke(fn, dptr->ts(), dptr->records(*dict_ptr));
  }
}


}}} /* namespace monsoon::history::v2 */

#endif /* V2_TSDATA_LIST_INL_H */
