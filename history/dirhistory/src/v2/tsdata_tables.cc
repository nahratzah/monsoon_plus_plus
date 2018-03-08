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
#include <type_traits>
#include <algorithm>
#include <queue>
#include <monsoon/objpipe/callback.h>
#include <monsoon/objpipe/merge.h>

namespace monsoon {
namespace history {
namespace v2 {


tsdata_v2_tables::tsdata_v2_tables(file_segment<file_data_tables>&& data,
    const tsdata_v2::carg& constructor_arg)
: tsdata_v2(constructor_arg),
  data_(std::move(data))
{}

tsdata_v2_tables::~tsdata_v2_tables() noexcept {}

std::shared_ptr<io::fd> tsdata_v2_tables::fd() const noexcept {
  return data_.ctx().fd();
}

bool tsdata_v2_tables::is_writable() const noexcept {
  return false;
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

void tsdata_v2_tables::push_back(const time_series&) {
  throw std::runtime_error("unsupported");
}

namespace {


auto emit_type_less = [](const tsdata_v2_tables::emit_type& x, const tsdata_v2_tables::emit_type& y) noexcept -> bool {
  return std::get<0>(x) < std::get<0>(y); // Compare time points.
};

auto emit_type_merge = [](tsdata_v2_tables::emit_type&& x, tsdata_v2_tables::emit_type&& y) -> tsdata_v2_tables::emit_type&& {
#if __cplusplus >= 201703
  std::get<1>(x).merge(std::get<1>(y));
#else
  std::copy(std::get<1>(y).begin(), std::get<1>(y).end(), std::inserter(std::get<1>(x), std::get<1>(x).end()));
#endif
  return std::move(x);
};

auto emit_fdtblock_pipe(
    std::optional<time_point> tr_begin,
    std::optional<time_point> tr_end,
    std::shared_ptr<const std::vector<time_point>> time_points_ptr,
    group_name group_name_ptr,
    metric_name metric_name_ptr,
    std::shared_ptr<const std::vector<std::optional<metric_value>>> mv_column_ptr)
-> decltype(auto) {
  using emit_type = tsdata_v2_tables::emit_type;

  return objpipe::new_callback<emit_type>(
      [=](auto cb) {
        auto tp_iter = time_points_ptr->begin();
        auto mv_iter = mv_column_ptr->begin();

        for (;
            tp_iter != time_points_ptr->end() && mv_iter != mv_column_ptr->end();
            ++tp_iter, ++mv_iter) {
          if (tr_begin.has_value() && *tp_iter < *tr_begin)
            continue;
          if (tr_end.has_value() && *tp_iter > *tr_end)
            continue;

          if (mv_iter->has_value()) {
            std::tuple_element_t<1, emit_type> map;
            map.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(group_name_ptr, metric_name_ptr),
                std::forward_as_tuple(**mv_iter));
            cb(emit_type(*tp_iter, std::move(map)));
          }
        }
      });
}

using emit_fdtblock_pipe_t = decltype(
    emit_fdtblock_pipe(
        std::declval<std::optional<time_point>>(),
        std::declval<std::optional<time_point>>(),
        std::declval<std::shared_ptr<const std::vector<time_point>>>(),
        std::declval<group_name>(),
        std::declval<metric_name>(),
        std::declval<std::shared_ptr<const std::vector<std::optional<metric_value>>>>()));

auto emit_fdtblock(
    std::shared_ptr<const file_data_tables_block> block,
    std::optional<time_point> tr_begin, std::optional<time_point> tr_end,
    const path_matcher& group_filter,
    const tag_matcher& tag_filter,
    const path_matcher& metric_filter)
-> objpipe::reader<tsdata_v2_tables::emit_type> {
  using emit_type = tsdata_v2_tables::emit_type;

  auto time_points_ptr = std::shared_ptr<const std::vector<time_point>>(block, &std::get<0>(*block));
  const std::shared_ptr<const tables> tbl_ptr = std::get<1>(*block).get();

  std::vector<emit_fdtblock_pipe_t> columns;
  for (const auto& tbl_entry : *tbl_ptr) {
    if (!group_filter(std::get<0>(tbl_entry).get_path())
        || !tag_filter(std::get<0>(tbl_entry).get_tags()))
      continue; // SKIP

    auto group_name_ptr = std::get<0>(tbl_entry);
    auto group_table_ptr = std::get<1>(tbl_entry).get();

    for (const auto& mv_map_entry : std::get<1>(*group_table_ptr)) {
      if (!metric_filter(mv_map_entry.first))
        continue; // SKIP

      auto metric_name_ptr = mv_map_entry.first;
      std::shared_ptr<const std::vector<std::optional<metric_value>>> mv_column_ptr = mv_map_entry.second.get();

      columns.emplace_back(
          emit_fdtblock_pipe(
              tr_begin, tr_end,
              time_points_ptr,
              group_name_ptr,
              metric_name_ptr,
              mv_column_ptr));
    }
  }

  return objpipe::merge_combine(
      std::make_move_iterator(columns.begin()), std::make_move_iterator(columns.end()),
      emit_type_less,
      emit_type_merge);
}

using emit_fdtblock_t = decltype(
    emit_fdtblock(
        std::declval<std::shared_ptr<const file_data_tables_block>>(),
        std::declval<std::optional<time_point>>(),
        std::declval<std::optional<time_point>>(),
        std::declval<const path_matcher&>(),
        std::declval<const tag_matcher&>(),
        std::declval<const path_matcher&>()));


} /* namespace <monsoon::history::v2::<unnamed> */

auto tsdata_v2_tables::emit(
    std::optional<time_point> tr_begin, std::optional<time_point> tr_end,
    const path_matcher& group_filter,
    const tag_matcher& tag_filter,
    const path_matcher& metric_filter)
    const
-> objpipe::reader<emit_type> {
  const std::shared_ptr<const file_data_tables> file_data_tables =
      data_.get();

  auto block_chain = objpipe::new_callback<emit_fdtblock_t>(
      [file_data_tables, tr_begin, tr_end, group_filter, tag_filter, metric_filter](auto cb) {
        for (const file_data_tables_block& block : *file_data_tables) {
          cb(emit_fdtblock(
                  std::shared_ptr<const file_data_tables_block>(file_data_tables, &block),
                  tr_begin, tr_end, group_filter, tag_filter, metric_filter));
        }
      });

  if (is_sorted() && is_distinct()) { // Operate on sequential blocks.
    return block_chain.iterate();
  } else { // Parallel iteration of blocks.
    if (is_distinct()) { // Merge only.
      return objpipe::merge(
          block_chain.begin(),
          block_chain.end(),
          emit_type_less);
    } else {
      return objpipe::merge_combine(
          block_chain.begin(),
          block_chain.end(),
          emit_type_less,
          emit_type_merge);
    }
  }
}

auto tsdata_v2_tables::emit_time(
    std::optional<time_point> tr_begin, std::optional<time_point> tr_end)
    const
-> objpipe::reader<time_point> {
  using iterator =
      typename std::tuple_element_t<0, file_data_tables_block>::const_iterator;

  const std::shared_ptr<const file_data_tables> file_data_tables =
      data_.get();

  if (is_sorted() && is_distinct()) { // Operate on sequential blocks.
    return objpipe::new_callback<time_point>(
        [file_data_tables, tr_begin, tr_end](auto cb) {
          for (const file_data_tables_block& block : *file_data_tables) {
            iterator b = std::get<0>(block).begin();
            iterator e = std::get<0>(block).end();
            if (tr_begin.has_value())
              b = std::lower_bound(b, e, tr_begin.value());
            if (tr_end.has_value())
              e = std::upper_bound(b, e, tr_end.value());

            std::for_each(b, e, cb);
          }
        });
  } else {
    auto callback_factory =
        [&](const file_data_tables_block& block) {
          auto block_ptr = std::shared_ptr<const file_data_tables_block>(file_data_tables, &block);
          return objpipe::new_callback<time_point>(
              [block_ptr, tr_begin, tr_end](auto cb) {
                iterator b = std::get<0>(*block_ptr).begin();
                iterator e = std::get<0>(*block_ptr).end();
                if (tr_begin.has_value())
                  b = std::lower_bound(b, e, tr_begin.value());
                if (tr_end.has_value())
                  e = std::upper_bound(b, e, tr_end.value());

                std::for_each(b, e, cb);
              });
        };
    using callback_factory_type = std::decay_t<decltype(callback_factory(std::declval<const file_data_tables_block&>()))>;

    std::vector<callback_factory_type> parallel;
    for (const file_data_tables_block& block : *file_data_tables)
      parallel.emplace_back(callback_factory(block));

    if (is_distinct()) { // Merge only.
      return objpipe::merge(
          std::make_move_iterator(parallel.begin()),
          std::make_move_iterator(parallel.end()));
    } else {
      return objpipe::merge_combine(
          std::make_move_iterator(parallel.begin()),
          std::make_move_iterator(parallel.end()),
          std::less<>(),
          [](const auto& x, [[maybe_unused]] const auto& y) { return x; });
    }
  }
}


}}} /* namespace monsoon::history::v2 */
