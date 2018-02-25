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

class monsoon_dirhistory_local_ metric_iteration {
 public:
  metric_iteration() = default;
  metric_iteration(const metric_iteration&) = default;
  metric_iteration& operator=(const metric_iteration&) = default;

  metric_iteration(metric_iteration&& o) noexcept
  : group(std::move(o.group)),
    metric(std::move(o.metric)),
    table(std::move(o.table)),
    b_(std::move(o.b_)),
    e_(std::move(o.e_))
  {}

  metric_iteration& operator=(metric_iteration&& o) noexcept {
    group = std::move(o.group);
    metric = std::move(o.metric);
    table = std::move(o.table);
    b_ = std::move(o.b_);
    e_ = std::move(o.e_);
    return *this;
  }

  metric_iteration(
      std::shared_ptr<const group_name> group,
      std::shared_ptr<const metric_name> metric,
      std::shared_ptr<const metric_table> table)
  : group(std::move(group)),
    metric(std::move(metric)),
    table(std::move(table))
  {
    if (table != nullptr)
      std::tie(b_, e_) = std::forward_as_tuple(table->begin(), table->end());
  }

  std::shared_ptr<const group_name> group;
  std::shared_ptr<const metric_name> metric;
  std::shared_ptr<const metric_table> table;

  explicit operator bool() const noexcept {
    return table != nullptr && b_ != e_;
  }

  auto next()
  -> std::optional<std::tuple<const group_name&, const metric_name&, const metric_value&>> {
    assert(group != nullptr && metric != nullptr && table != nullptr);
    assert(b_ != e_);
    const std::optional<metric_value> opt_mv = *b_++;
    if (!opt_mv.has_value()) return {};
    return std::tie(*group, *metric, opt_mv.value());
  }

 private:
  metric_table::const_iterator b_, e_;
};

template<typename Callback>
void monsoon_dirhistory_local_ emit_fdtblock(
    std::shared_ptr<const file_data_tables_block> block,
    Callback& cb,
    std::optional<time_point> tr_begin, std::optional<time_point> tr_end,
    const path_matcher& group_filter,
    const tag_matcher& tag_filter,
    const path_matcher& metric_filter) {
  auto timestamps = std::get<0>(*block);
  std::vector<metric_iteration> data;

  // Build parallel iterators per each selected metric.
  const std::shared_ptr<const tables> tbl_ptr = std::get<1>(*block).get();
  std::for_each(tbl_ptr->begin(), tbl_ptr->end(),
      [&data, &tbl_ptr, &group_filter, &tag_filter, &metric_filter](const auto& tbl_entry) {
        if (!group_filter(std::get<0>(tbl_entry).get_path())
            || !tag_filter(std::get<0>(tbl_entry).get_tags()))
          return; // SKIP

        const std::shared_ptr<const group_name> group_ptr =
            std::shared_ptr<const group_name>(tbl_ptr, &std::get<0>(tbl_entry));
        std::shared_ptr<const group_table> gr_tbl = std::get<1>(tbl_entry).get();
        const auto metric_map =
            std::shared_ptr<std::tuple_element_t<1, const group_table>>(
                gr_tbl,
                &std::get<1>(*gr_tbl));

        std::for_each(metric_map->begin(), metric_map->end(),
            [&data, &group_ptr, &metric_map, &metric_filter](
                const auto& metric_map_entry) {
              if (!metric_filter(metric_map_entry.first))
                return; // SKIP

              const std::shared_ptr<const metric_name> metric_ptr =
                  std::shared_ptr<const metric_name>(
                      metric_map,
                      &metric_map_entry.first);

              data.emplace_back(
                  group_ptr,
                  metric_ptr,
                  metric_map_entry.second.get());
            });
      });

  // Emit timestamps and parallel iterators.
  auto emit = tsdata_v2_tables::emit_type();
  for (const time_point& tp : timestamps) {
    std::get<0>(emit) = tp;
    std::get<1>(emit).clear();
    std::get<1>(emit).reserve(data.size());

    std::for_each(data.begin(), data.end(),
        [&emit](auto& iter) {
          if (iter) {
            auto opt_tpl = iter.next();
            if (opt_tpl.has_value()) {
              std::get<1>(emit).emplace(
                  std::piecewise_construct,
                  std::forward_as_tuple(
                      std::get<0>(std::move(opt_tpl.value())),
                      std::get<1>(std::move(opt_tpl.value()))),
                  std::forward_as_tuple(
                      std::get<2>(std::move(opt_tpl.value()))));
            }
          }
        });

    if ((!tr_begin.has_value() || tp >= tr_begin)
        && (!tr_end.has_value() || tp <= tr_end))
      cb(std::move(emit));
  }
}

auto tsdata_v2_tables::emit(
    std::optional<time_point> tr_begin, std::optional<time_point> tr_end,
    const path_matcher& group_filter,
    const tag_matcher& tag_filter,
    const path_matcher& metric_filter)
    const
-> objpipe::reader<emit_type> {
  const std::shared_ptr<const file_data_tables> file_data_tables =
      data_.get();

  if (is_sorted() && is_distinct()) { // Operate on sequential blocks.
    return objpipe::new_callback<emit_type>(
        [file_data_tables, tr_begin, tr_end, group_filter, tag_filter, metric_filter](auto cb) {
          for (const file_data_tables_block& block : *file_data_tables) {
            emit_fdtblock(
                std::shared_ptr<const file_data_tables_block>(file_data_tables, &block),
                cb,
                tr_begin, tr_end, group_filter, tag_filter, metric_filter);
          }
        });
  } else { // Parallel iteration of blocks.
    auto callback_factory =
        [&](const file_data_tables_block& block) {
          auto block_ptr = std::shared_ptr<const file_data_tables_block>(file_data_tables, &block);
          return objpipe::new_callback<emit_type>(
              [block_ptr, tr_begin, tr_end, group_filter, tag_filter, metric_filter](auto cb) {
                emit_fdtblock(
                    block_ptr,
                    cb,
                    tr_begin, tr_end, group_filter, tag_filter, metric_filter);
              });
        };
    using callback_factory_type = std::decay_t<decltype(callback_factory(std::declval<const file_data_tables_block&>()))>;

    auto less = [](const emit_type& x, const emit_type& y) noexcept {
      return std::get<0>(x) < std::get<0>(y);
    };

    std::vector<callback_factory_type> parallel;
    for (const file_data_tables_block& block : *file_data_tables)
      parallel.emplace_back(callback_factory(block));

    if (is_distinct()) { // Merge only.
      return objpipe::merge(
          std::make_move_iterator(parallel.begin()),
          std::make_move_iterator(parallel.end()),
          std::move(less));
    } else {
      auto combine = [](emit_type& x, emit_type&& y) -> emit_type& {
#if __cplusplus >= 201703
        std::get<1>(x).merge(std::get<1>(std::move(y)));
#else
        std::copy(
            std::get<1>(y).begin(),
            std::get<1>(y).end(),
            std::inserter(std::get<1>(x), std::get<1>(x).end()));
#endif
        return x;
      };

      return objpipe::merge_combine(
          std::make_move_iterator(parallel.begin()),
          std::make_move_iterator(parallel.end()),
          std::move(less),
          std::move(combine));
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
