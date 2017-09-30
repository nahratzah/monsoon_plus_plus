#include "tsdata.h"
#include "encdec.h"
#include <functional>
#include <algorithm>
#include <monsoon/time_series.h>
#include <monsoon/time_series_value.h>
#include <monsoon/time_point.h>
#include <monsoon/group_name.h>
#include <monsoon/metric_name.h>
#include <monsoon/metric_value.h>

namespace monsoon {
namespace history {
namespace v2 {


tsdata_v2::~tsdata_v2() noexcept {}

std::vector<time_series> tsdata_v2::read_all() const {
  using namespace std::placeholders;

  std::vector<time_series> result = read_all_raw_();

  // only sort if flag sorted is false
  if ((flags_ & header_flags::SORTED) == 0u) {
    std::stable_sort(result.begin(), result.end(),
        std::bind(std::less<time_point>(),
            std::bind(&time_series::get_time, _1),
            std::bind(&time_series::get_time, _2)));
  }

  // only merge if flag distinct is false
  if ((flags_ & header_flags::DISTINCT) == 0u) {
    // Merge successors with same timestamp together.
    auto result_back_iter = result.begin();
    auto result_end = result.end(); // New end of result.
    if (result_back_iter != result_end) {
      while (result_back_iter != result_end - 1) {
        if (result_back_iter[0].get_time() != result_back_iter[1].get_time()) {
          ++result_back_iter;
        } else {
          // Merge time_series.
          for (auto tsv_iter = std::make_move_iterator(result_back_iter[1].data().begin());
               tsv_iter != std::make_move_iterator(result_back_iter[1].data().end());
               ++tsv_iter) {
            bool tsv_inserted;
            time_series::tsv_set::iterator tsv_pos;
            std::tie(tsv_pos, tsv_inserted) = result_back_iter[0].data().insert(*tsv_iter);

            if (!tsv_inserted) { // Merge time_series_value into existing.
              auto tmp = *tsv_pos;
              result_back_iter[0].data().erase(tsv_pos);

              for (auto metric_iter = std::make_move_iterator(tsv_iter->get_metrics().begin());
                  metric_iter != std::make_move_iterator(tsv_iter->get_metrics().end());
                  ++metric_iter) {
                tmp.metrics()[metric_iter->first] = std::move(metric_iter->second);
              }

              bool replaced;
              std::tie(std::ignore, replaced) = result_back_iter[0].data().insert(std::move(tmp));
              assert(replaced);
            }
          }

          if (result_back_iter + 2 != result_end)
            std::rotate(result_back_iter + 1, result_back_iter + 2, result_end);
          --result_end;
        }
      }

      // Erase elements shifted backwards for deletion.
      result.erase(result_end, result.end());
    }
  }

  return result;
}


}}} /* namespace monsoon::history::v2 */
