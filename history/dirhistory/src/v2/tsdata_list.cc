#include "tsdata_list.h"
#include "../raw_file_segment_writer.h"

namespace monsoon {
namespace history {
namespace v2 {


tsdata_v2_list::tsdata_v2_list(file_segment<tsdata_list>&& data,
    const tsdata_v2::carg& constructor_arg)
: tsdata_v2(constructor_arg),
  data_(std::move(data))
{}

tsdata_v2_list::~tsdata_v2_list() noexcept {}

std::shared_ptr<io::fd> tsdata_v2_list::fd() const noexcept {
  return data_.ctx().fd();
}

bool tsdata_v2_list::is_writable() const noexcept {
  return fd()->can_write();
}

std::vector<time_series> tsdata_v2_list::read_all_raw_() const {
  std::vector<time_series> result;
  visit(
      [&result](const time_point& ts, std::shared_ptr<const tsdata_list::record_array> records) {
        time_series::tsv_set tsv_set;
        std::transform(records->begin(), records->end(),
            std::inserter(tsv_set, tsv_set.end()),
            [](const auto& group_entry) {
              return time_series_value(
                  std::get<0>(group_entry), // Group name
                  *std::get<1>(group_entry).get()); // Metrics map
            });
        result.push_back(time_series(ts, std::move(tsv_set)));
      });
  return result;
}

void tsdata_v2_list::push_back(const time_series& ts) {
  encdec_writer out = encdec_writer(data_.ctx(), hdr_file_size());

  dictionary_delta dict;
  std::optional<file_segment_ptr> tsdata_pred;
  if (data_.file_ptr().offset() != 0u) {
    dict = *data_.get()->dictionary();
    tsdata_pred = data_.file_ptr();
  }
  assert(!dict.update_pending());

  const file_segment_ptr tsfile_ptr =
      encode_tsdata(out, ts, std::move(dict), std::move(tsdata_pred));

  out.ctx().fd()->flush();
  update_hdr(ts.get_time(), ts.get_time(), tsfile_ptr, out.offset());
  data_.update_addr(tsfile_ptr);
}

std::unordered_set<simple_group> tsdata_v2_list::simple_groups() const {
  std::unordered_set<simple_group> result;
  visit(
      [&result](const time_point& ts, std::shared_ptr<const tsdata_list::record_array> records) {
        std::transform(records->begin(), records->end(),
            std::inserter(result, result.end()),
            [](const auto& group_entry) {
              return std::get<0>(group_entry).get_path();
            });
      });
  return result;
}

std::unordered_set<group_name> tsdata_v2_list::group_names() const {
  std::unordered_set<group_name> result;
  visit(
      [&result](const time_point& ts, std::shared_ptr<const tsdata_list::record_array> records) {
        std::transform(records->begin(), records->end(),
            std::inserter(result, result.end()),
            [](const auto& group_entry) {
              return std::get<0>(group_entry);
            });
      });
  return result;
}

auto tsdata_v2_list::untagged_metrics() const
-> std::unordered_set<std::tuple<simple_group, metric_name>, metrics_hash> {
  std::unordered_set<std::tuple<simple_group, metric_name>, metrics_hash> result;
  visit(
      [&result](const time_point& ts, std::shared_ptr<const tsdata_list::record_array> records) {
        std::for_each(records->begin(), records->end(),
            [&result](const auto& group_entry) {
              const auto& name = std::get<0>(group_entry).get_path();
              const auto mmap = std::get<1>(group_entry).get();
              std::transform(mmap->begin(), mmap->end(),
                  std::inserter(result, result.end()),
                  [&name](const auto& metric_entry) {
                    return std::make_pair(name, std::get<0>(metric_entry));
                  });
            });
      });
  return result;
}

auto tsdata_v2_list::tagged_metrics() const
-> std::unordered_set<std::tuple<group_name, metric_name>, metrics_hash> {
  std::unordered_set<std::tuple<group_name, metric_name>, metrics_hash> result;
  visit(
      [&result](const time_point& ts, std::shared_ptr<const tsdata_list::record_array> records) {
        std::for_each(records->begin(), records->end(),
            [&result](const auto& group_entry) {
              const auto& name = std::get<0>(group_entry);
              const auto mmap = std::get<1>(group_entry).get();
              std::transform(mmap->begin(), mmap->end(),
                  std::inserter(result, result.end()),
                  [&name](const auto& metric_entry) {
                    return std::make_pair(name, std::get<0>(metric_entry));
                  });
            });
      });
  return result;
}

void tsdata_v2_list::emit(
    emit_acceptor<group_name, metric_name, metric_value>& accept_fn,
    std::optional<time_point> tr_begin, std::optional<time_point> tr_end,
    const std::unordered_multimap<group_name, metric_name>& filter) const {
  struct group_metric_hash {
    std::size_t operator()(const std::tuple<group_name, metric_name>& t) const {
      return std::hash<group_name>()(std::get<group_name>(t))
          ^ std::hash<metric_name>()(std::get<metric_name>(t));
    }
  };
  std::unordered_set<std::tuple<group_name, metric_name>, group_metric_hash>
      metric_filter;
  std::copy(filter.begin(), filter.end(),
      std::inserter(metric_filter, metric_filter.end()));

  emit_(
      accept_fn, tr_begin, tr_end,
      [&filter](const group_name& gn) {
        return filter.find(gn) != filter.end();
      },
      [&metric_filter](const group_name& gn, const metric_name& mn) {
        return metric_filter.find(std::tie(gn, mn)) != metric_filter.end();
      });
}

void tsdata_v2_list::emit(
    emit_acceptor<group_name, metric_name, metric_value>& accept_fn,
    std::optional<time_point> tr_begin, std::optional<time_point> tr_end,
    const std::unordered_multimap<simple_group, metric_name>& filter) const {
  struct group_metric_hash {
    std::size_t operator()(const std::tuple<simple_group, metric_name>& t) const {
      return std::hash<simple_group>()(std::get<simple_group>(t))
          ^ std::hash<metric_name>()(std::get<metric_name>(t));
    }
  };
  std::unordered_set<std::tuple<simple_group, metric_name>, group_metric_hash>
      metric_filter;
  std::copy(filter.begin(), filter.end(),
      std::inserter(metric_filter, metric_filter.end()));

  emit_(
      accept_fn, tr_begin, tr_end,
      [&filter](const group_name& gn) {
        return filter.find(gn.get_path()) != filter.end();
      },
      [&metric_filter](const group_name& gn, const metric_name& mn) {
        return (metric_filter.find(std::tie(gn.get_path(), mn))
            != metric_filter.end());
      });
}

void tsdata_v2_list::emit_(
    emit_acceptor<group_name, metric_name, metric_value>& accept_fn,
    std::optional<time_point> tr_begin, std::optional<time_point> tr_end,
    std::function<bool(const group_name&)> group_filter,
    std::function<bool(const group_name&, const metric_name&)> filter) const {
  using vector_type =
      emit_acceptor<group_name, metric_name, metric_value>::vector_type;

  visit(
      [&accept_fn, &group_filter, &filter](const time_point& ts,
          std::shared_ptr<const tsdata_list::record_array> records) {
        vector_type emit_data;

        std::for_each(records->begin(), records->end(),
            [&emit_data, &group_filter, &filter](const auto& group_entry) {
              const auto& group_name = std::get<0>(group_entry);

              if (group_filter(group_name)) {
                const auto metrics = std::get<1>(group_entry).get();
                std::for_each(metrics->begin(), metrics->end(),
                    [&group_name, &emit_data, &filter](
                        const auto& metric_entry) {
                      const auto& metric_name = std::get<0>(metric_entry);
                      const auto& metric_value = std::get<1>(metric_entry);

                      if (filter(group_name, metric_name)) {
                        emit_data.emplace_back(
                            group_name,
                            metric_name,
                            metric_value);
                      }
                    });
              }
            });

        accept_fn.accept(ts, std::move(emit_data));
      });
}


}}} /* namespace monsoon::history::v2 */
