#include "tsdata_tables.h"
#include "encdec.h"
#include <memory>
#include <vector>
#include <monsoon/group_name.h>
#include <monsoon/metric_name.h>
#include <monsoon/time_series_value.h>
#include <monsoon/time_series.h>
#include <monsoon/time_point.h>
#include <tuple>
#include <functional>
#include <utility>
#include <iterator>

namespace monsoon {
namespace history {
namespace v2 {


tsdata_v2_tables::~tsdata_v2_tables() noexcept {}

std::shared_ptr<io::fd> tsdata_v2_tables::fd() const noexcept {
  return data_.ctx().fd();
}

std::vector<time_series> tsdata_v2_tables::read_all() const {
  using namespace std::placeholders;
  using std::swap;

  std::vector<time_series> result;

  const std::shared_ptr<const file_data_tables> file_data_tables =
      data_.get();
  for (const file_data_tables_block& block : *file_data_tables) {
    const std::vector<time_point>& timestamps = std::get<0>(block);
    const std::shared_ptr<const tables> tbl = std::get<1>(block).get();

    auto tsdata = std::vector<time_series::tsv_set>(timestamps.size());
    for (tables::const_reference tbl_grp : *tbl) {
      const std::shared_ptr<const group_table> grp_data =
          std::get<1>(tbl_grp).get();

      const std::vector<bool>& presence = std::get<0>(*grp_data);

      // Fill mmap with metric values over time.
      auto mmap = std::vector<time_series_value::metric_map>(presence.size());
      for (const auto& metric_entry : std::get<1>(*grp_data)) {
        const metric_name& mname = std::get<0>(metric_entry);
        const std::shared_ptr<const metric_table> mtbl =
            std::get<1>(metric_entry).get();

        // Fill mmap with a specific metric value over time.
        for (auto iter = std::make_tuple(mtbl->cbegin(), mmap.begin());
            std::get<0>(iter) != mtbl->cend() && std::get<1>(iter) != mmap.end();
            ++std::get<0>(iter), ++std::get<1>(iter)) {
          auto& out_map = *std::get<1>(iter);
          const auto& opt_mv = *std::get<0>(iter);
          if (opt_mv.has_value()) out_map.emplace(mname, opt_mv.value());
        }
      }

      // Add group over time.
      const group_name& grp = std::get<0>(tbl_grp);
      for (auto iter = std::make_tuple(
              presence.cbegin(),
              std::make_move_iterator(mmap.begin()),
              tsdata.begin());
          (std::get<0>(iter) != presence.cend()
           && std::get<1>(iter) != std::make_move_iterator(mmap.end())
           && std::get<2>(iter) != tsdata.end());
          ++std::get<0>(iter), ++std::get<1>(iter), ++std::get<2>(iter)) {
        if (*std::get<0>(iter))
          std::get<2>(iter)->emplace(time_series_value(grp, *std::get<1>(iter)));
      }
    }

    // Add a new time series to the collection.
    std::transform(std::make_move_iterator(tsdata.begin()), std::make_move_iterator(tsdata.end()), std::make_move_iterator(timestamps.begin()), std::back_inserter(result),
        [](auto&& set, auto&& ts) {
          return time_series(std::move(ts), std::move(set));
        });
  }

  // XXX only if flag sorted is false
  std::stable_sort(result.begin(), result.end(),
      std::bind(std::less<time_point>(),
          std::bind(&time_series::get_time, _1),
          std::bind(&time_series::get_time, _2)));
  // XXX only if flag distinct is false
  {
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
