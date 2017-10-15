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
#include <boost/coroutine2/coroutine.hpp>
#include <boost/coroutine2/protected_fixedsize_stack.hpp>

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

std::unordered_set<simple_group> tsdata_v2_tables::simple_groups() const {
  std::unordered_set<simple_group> result;

  const std::shared_ptr<const file_data_tables> file_data_tables =
      data_.get();
  for (const file_data_tables_block& block : *file_data_tables) {
    const std::shared_ptr<const tables> tbl = std::get<1>(block).get();
    std::transform(tbl->begin(), tbl->end(),
        std::inserter(result, result.end()),
        [](const auto& tbl_entry) {
          return std::get<0>(tbl_entry).get_path();
        });
  }

  return result;
}

std::unordered_set<group_name> tsdata_v2_tables::group_names() const {
  std::unordered_set<group_name> result;

  const std::shared_ptr<const file_data_tables> file_data_tables =
      data_.get();
  for (const file_data_tables_block& block : *file_data_tables) {
    const std::shared_ptr<const tables> tbl = std::get<1>(block).get();
    std::transform(tbl->begin(), tbl->end(),
        std::inserter(result, result.end()),
        [](const auto& tbl_entry) {
          return std::get<0>(tbl_entry);
        });
  }

  return result;
}

auto tsdata_v2_tables::untagged_metrics() const
-> std::unordered_set<std::tuple<simple_group, metric_name>, metrics_hash> {
  std::unordered_set<std::tuple<simple_group, metric_name>, metrics_hash> result;

  const std::shared_ptr<const file_data_tables> file_data_tables =
      data_.get();
  for (const file_data_tables_block& block : *file_data_tables) {
    const std::shared_ptr<const tables> tbl = std::get<1>(block).get();
    std::for_each(tbl->begin(), tbl->end(),
        [&result](const auto& tbl_entry) {
          const auto& name = std::get<0>(tbl_entry).get_path();
          const auto grp_table = std::get<1>(tbl_entry).get();
          std::transform(std::get<1>(*grp_table).begin(), std::get<1>(*grp_table).end(),
              std::inserter(result, result.end()),
              [&name](const auto& metric_tbl_entry) {
                return std::make_pair(name, std::get<0>(metric_tbl_entry));
              });
        });
  }

  return result;
}

auto tsdata_v2_tables::tagged_metrics() const
-> std::unordered_set<std::tuple<group_name, metric_name>, metrics_hash> {
  std::unordered_set<std::tuple<group_name, metric_name>, metrics_hash> result;

  const std::shared_ptr<const file_data_tables> file_data_tables =
      data_.get();
  for (const file_data_tables_block& block : *file_data_tables) {
    const std::shared_ptr<const tables> tbl = std::get<1>(block).get();
    std::for_each(tbl->begin(), tbl->end(),
        [&result](const auto& tbl_entry) {
          const auto& name = std::get<0>(tbl_entry);
          const auto grp_table = std::get<1>(tbl_entry).get();
          std::transform(std::get<1>(*grp_table).begin(), std::get<1>(*grp_table).end(),
              std::inserter(result, result.end()),
              [&name](const auto& metric_tbl_entry) {
                return std::make_pair(name, std::get<0>(metric_tbl_entry));
              });
        });
  }

  return result;
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

void monsoon_dirhistory_local_ emit_fdtblock(
    std::shared_ptr<const file_data_tables_block> block,
    std::function<void(time_point, tsdata_v2_tables::emit_map&&)> cb,
    std::optional<time_point> tr_begin, std::optional<time_point> tr_end,
    const std::function<bool(const group_name&)>& group_filter,
    const std::function<bool(const group_name&, const metric_name&)>& metric_filter) {
  auto timestamps = std::get<0>(*block);
  std::vector<metric_iteration> data;

  // Build parallel iterators per each selected metric.
  const std::shared_ptr<const tables> tbl_ptr = std::get<1>(*block).get();
  std::for_each(tbl_ptr->begin(), tbl_ptr->end(),
      [&data, &tbl_ptr, &group_filter, &metric_filter](const auto& tbl_entry) {
        if (!group_filter(std::get<0>(tbl_entry)))
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
              if (!metric_filter(*group_ptr, metric_map_entry.first))
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
  auto emit = tsdata_v2_tables::emit_map();
  for (const time_point& tp : timestamps) {
    emit.clear();
    emit.reserve(data.size());

    std::for_each(data.begin(), data.end(),
        [&emit](auto& iter) {
          if (iter) {
            auto opt_tpl = iter.next();
            if (opt_tpl.has_value()) {
              emit.emplace(
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
      cb(tp, std::move(emit));
  }
}

void tsdata_v2_tables::emit(
    const std::function<void(time_point, emit_map&&)>& accept_fn,
    std::optional<time_point> tr_begin, std::optional<time_point> tr_end,
    const std::function<bool(const group_name&)>& group_filter,
    const std::function<bool(const group_name&, const metric_name&)>& metric_filter)
    const {
  const std::shared_ptr<const file_data_tables> file_data_tables =
      data_.get();

  if (is_sorted() && is_distinct()) { // Operate on sequential blocks.
    using namespace std::placeholders;

    for (const file_data_tables_block& block : *file_data_tables) {
      emit_fdtblock(
          std::shared_ptr<const file_data_tables_block>(file_data_tables, &block),
          accept_fn,
          tr_begin, tr_end, group_filter, metric_filter);
    }
  } else { // parallel iteration of blocks.
    // Declare parallel iteration
    class iterators_value_type {
     public:
      using co_arg_type = std::tuple<time_point, emit_map>;
      using co_type = boost::coroutines2::coroutine<co_arg_type>;

      iterators_value_type() = default;

      iterators_value_type(iterators_value_type&& o) noexcept
      : val_(std::move(o.val_)),
        co_(std::move(o.co_))
      {}

      iterators_value_type& operator=(iterators_value_type&& o) noexcept {
        val_ = std::move(o.val_);
        co_ = std::move(o.co_);
        return *this;
      }

      iterators_value_type(
          std::shared_ptr<const file_data_tables_block> block,
          std::optional<time_point> tr_begin, std::optional<time_point> tr_end,
          const std::function<bool(const group_name&)>& group_filter,
          const std::function<bool(const group_name&, const metric_name&)>& metric_filter)
      : co_(boost::coroutines2::protected_fixedsize_stack(),
          [block, tr_begin, tr_end, &group_filter, &metric_filter](
              co_type::push_type& yield) {
            emit_fdtblock(
                block,
                [&yield](time_point tp, emit_map&& v) {
                  yield(co_arg_type(std::move(tp), std::move(v)));
                },
                tr_begin, tr_end, group_filter, metric_filter);
          })
      {
        if (co_) val_ = co_.get();
      }

      explicit operator bool() const noexcept {
        return bool(co_);
      }

      co_arg_type& get() {
        assert(val_.has_value());
        return val_.value();
      }

      time_point ts() const {
        assert(val_.has_value());
        return std::get<0>(val_.value());
      }

      void advance() {
        co_();
        if (co_)
          val_ = co_.get();
        else
          val_ = {};
      }

     private:
      std::optional<co_arg_type> val_;
      mutable co_type::pull_type co_;
    };
    struct iterators_cmp {
      bool operator()(const iterators_value_type& x,
          const iterators_value_type& y) const {
        if (!x) return !y;
        if (!y) return false;
        return x.ts() > y.ts();
      }
    };
    auto iterators = std::vector<iterators_value_type>();
    iterators.reserve(file_data_tables->size());

    // Build parallel iteration.
    for (const file_data_tables_block& block : *file_data_tables) {
      iterators.emplace_back(
          std::shared_ptr<const file_data_tables_block>(file_data_tables, &block),
          tr_begin, tr_end, group_filter, metric_filter);
    }
    std::make_heap(iterators.begin(), iterators.end(), iterators_cmp());

    // Traverse iteration.
    time_point last_ts;
    emit_map last_val;
    while (!iterators.empty()) {
      std::pop_heap(iterators.begin(), iterators.end(), iterators_cmp());
      auto& top = iterators.back();
      if (!top) {
        iterators.pop_back();
        continue;
      }

      // Handle argument.
      auto& arg = top.get();
      if (last_val.empty()) { // First iteration.
        std::tie(last_ts, last_val) = arg;
      } else if (std::get<0>(arg) == last_ts) { // Merge with same-time entry.
#if __cplusplus >= 201703
        if (last_val.get_allocator() == std::get<1>(arg).get_allocator())
          last_val.merge(std::get<1>(std::move(arg)));
        else
#endif
          last_val.insert(
              std::make_move_iterator(std::get<1>(arg).begin()),
              std::make_move_iterator(std::get<1>(arg).end()));
      } else { // Emit previous and make current the 'last' value.
        accept_fn(std::move(last_ts), std::move(last_val));
        std::tie(last_ts, last_val) = arg;
      }

      top.advance(); // Advance co routine
      std::push_heap(iterators.begin(), iterators.end(), iterators_cmp());
    }
    if (!last_val.empty()) // Push last unpushed value
      accept_fn(std::move(last_ts), std::move(last_val));
  }
}

void tsdata_v2_tables::emit_time(
    const std::function<void(time_point)>& accept_fn,
    std::optional<time_point> tr_begin, std::optional<time_point> tr_end)
    const {
  using iterator =
      typename std::tuple_element_t<0, file_data_tables_block>::const_iterator;

  const std::shared_ptr<const file_data_tables> file_data_tables =
      data_.get();

  if (is_sorted() && is_distinct()) { // Operate on sequential blocks.
    using namespace std::placeholders;

    for (const file_data_tables_block& block : *file_data_tables) {
      iterator b = std::get<0>(block).begin();
      iterator e = std::get<0>(block).end();
      if (tr_begin.has_value())
        b = std::lower_bound(b, e, tr_begin.value());
      if (tr_end.has_value())
        e = std::upper_bound(b, e, tr_end.value());

      std::for_each(b, e, std::ref(accept_fn));
    }
  } else { // parallel iteration of blocks.
    using iter_pair = std::pair<iterator, iterator>;
    struct iter_heap_cmp {
      bool operator()(const iter_pair& x, const iter_pair& y) const {
        assert(x.first != x.second);
        assert(y.first != y.second);
        return *y.first > *x.first;
      }
    };
    using queue = std::priority_queue<iter_pair, std::vector<iter_pair>, iter_heap_cmp>;

    queue iters;
    for (const file_data_tables_block& block : *file_data_tables) {
      iterator b = std::get<0>(block).begin();
      iterator e = std::get<0>(block).end();
      if (tr_begin.has_value())
        b = std::lower_bound(b, e, tr_begin.value());
      if (tr_end.has_value())
        e = std::upper_bound(b, e, tr_end.value());
      if (b != e)
        iters.push(std::forward_as_tuple(std::move(b), std::move(e)));
    }

    while (!iters.empty()) {
      iter_pair top = iters.top();
      iters.pop();
      accept_fn(*top.first);
      ++top.first;
      if (top.first != top.second) iters.push(std::move(top));
    }
  }
}


}}} /* namespace monsoon::history::v2 */
