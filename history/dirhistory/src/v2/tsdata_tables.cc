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

std::vector<time_series> tsdata_v2_tables::read_all_raw_() const {
  using namespace std::placeholders;
  using std::swap;

  std::vector<time_series> result;
  std::vector<time_series::tsv_set> tsdata;
  std::vector<time_series_value::metric_map> mmap;

  const std::shared_ptr<const file_data_tables> file_data_tables =
      data_.get();
  for (const file_data_tables_block& block : *file_data_tables) {
    const std::vector<time_point>& timestamps = std::get<0>(block);
    const std::shared_ptr<const tables> tbl = std::get<1>(block).get();

    // Create time_series maps over time.
    tsdata.resize(timestamps.size());
    std::for_each(tsdata.begin(), tsdata.end(),
        std::bind(&time_series::tsv_set::clear, _1));
    for (tables::const_reference tbl_grp : *tbl) {
      const group_name& gname = std::get<0>(tbl_grp);
      const std::shared_ptr<const group_table> grp_data =
          std::get<1>(tbl_grp).get();
      const std::vector<bool>& presence = std::get<0>(*grp_data);

      // Fill mmap with metric values over time.
      mmap.resize(presence.size());
      std::for_each(mmap.begin(), mmap.end(),
          std::bind(&time_series_value::metric_map::clear, _1));
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
      for (auto iter = std::make_tuple(
              presence.cbegin(),
              std::make_move_iterator(mmap.begin()),
              tsdata.begin());
          (std::get<0>(iter) != presence.cend()
           && std::get<1>(iter) != std::make_move_iterator(mmap.end())
           && std::get<2>(iter) != tsdata.end());
          ++std::get<0>(iter), ++std::get<1>(iter), ++std::get<2>(iter)) {
        if (*std::get<0>(iter))
          std::get<2>(iter)->emplace(time_series_value(gname, *std::get<1>(iter)));
      }
    }

    // Add a new time series to the collection.
    std::transform(std::make_move_iterator(tsdata.begin()), std::make_move_iterator(tsdata.end()), std::make_move_iterator(timestamps.begin()), std::back_inserter(result),
        [](auto&& set, auto&& ts) {
          return time_series(std::move(ts), std::move(set));
        });
  }

  return result;
}


}}} /* namespace monsoon::history::v2 */
