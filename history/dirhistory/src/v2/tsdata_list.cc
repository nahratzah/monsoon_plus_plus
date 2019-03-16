#include "tsdata_list.h"
#include "../raw_file_segment_writer.h"
#include "tsdata_xdr.h"
#include "record_array.h"
#include "record_metrics.h"
#include <objpipe/callback.h>
#include <objpipe/of.h>
#include <objpipe/array.h>
#include <algorithm>

namespace monsoon {
namespace history {
namespace v2 {


tsdata_v2_list::~tsdata_v2_list() noexcept {}

bool tsdata_v2_list::is_writable() const noexcept {
  return fd().can_write();
}

auto tsdata_v2_list::read_() const
-> std::shared_ptr<tsdata_xdr> {
  return get_dynamics_cache<tsdata_xdr>(shared_from_this(), fdt());
}

std::vector<time_series> tsdata_v2_list::read_all_raw_() const {
  std::vector<std::shared_ptr<const tsdata_xdr>> records;
  for (auto ptr = read_();
      ptr != nullptr;
      ptr = ptr->get_predecessor())
    records.push_back(ptr);

  if (!is_sorted()) {
    std::stable_sort(
        records.begin(), records.end(),
        [](const auto& x_ptr, const auto& y_ptr) {
          return x_ptr->ts() > y_ptr->ts();
        });
  }

  auto pipe = objpipe::new_array(records.rbegin(), records.rend())
      .transform(
          [](std::shared_ptr<const tsdata_xdr> tsd) -> time_series {
            time_series::tsv_set data;
            for (record_array::value_type ra_value : *tsd->get()) {
              time_series_value::metric_map metrics;
              for (record_metrics::value_type rm_value : *ra_value)
                metrics.emplace(rm_value.name(), rm_value.get());
              data.emplace(ra_value.name(), std::move(metrics));
            }
            return time_series(tsd->ts(), std::move(data));
          });
  if (is_distinct())
    return std::move(pipe).to_vector();

  std::vector<time_series> result;
  std::move(pipe).for_each(
      [&result](time_series&& tsv) {
        // Trivial case: not a duplicate.
        if (result.empty() || result.back().get_time() != tsv.get_time()) {
          result.emplace_back(std::move(tsv));
          return;
        }

        std::unordered_map<group_name, time_series_value::metric_map> tmp;

        // First, add all entries from tsv.
        // We add those first, to allow them to override anything already present.
        std::transform(
            tsv.get_data().begin(),
            tsv.get_data().end(),
            std::inserter(tmp, tmp.end()),
            [](const time_series_value& tsv) {
              return std::make_pair(tsv.get_name(), tsv.get_metrics());
            });

        // Next, merge in all entries in result.back()
        // By merging these second, we prevent them from overriding already present metrics.
        std::for_each(
            result.back().get_data().begin(),
            result.back().get_data().end(),
            [&tmp](const time_series_value& tsv) {
              auto& metrics = tmp[tsv.get_name()];
              std::copy(
                  tsv.get_metrics().begin(),
                  tsv.get_metrics().end(),
                  std::inserter(metrics, metrics.end()));
            });

        // Replace contents of result.back with newly computed merged metrics.
        result.back().data().clear();
        std::transform(
            std::make_move_iterator(tmp.begin()),
            std::make_move_iterator(tmp.end()),
            std::inserter(result.back().data(), result.back().data().end()),
            [](auto&& entry) {
              return time_series_value(std::move(entry.first), std::move(entry.second));
            });
      });
  return result;
}

void tsdata_v2_list::push_back(const time_series& ts) {
  encdec_writer out = encdec_writer(get_ctx(), hdr_file_size());

  dictionary_delta dict;
  std::optional<file_segment_ptr> tsdata_pred;
  if (fdt() != file_segment_ptr()) {
    dict = *read_()->get_dictionary();
    tsdata_pred = fdt();
  }
  assert(!dict.update_pending());

  const file_segment_ptr tsfile_ptr =
      encode_tsdata(out, ts, std::move(dict), std::move(tsdata_pred));

  out.ctx().fd().flush();
  update_hdr(ts.get_time(), ts.get_time(), tsfile_ptr, out.offset());
}

void tsdata_v2_list::push_back(const emit_type& c) {
  push_back(make_time_series(c));
}

auto tsdata_v2_list::emit(
    std::optional<time_point> tr_begin, std::optional<time_point> tr_end,
    const path_matcher& group_filter,
    const tag_matcher& tag_filter,
    const path_matcher& metric_filter) const
-> objpipe::reader<emit_type> {
  std::shared_ptr<const tsdata_v2_list> self = shared_from_this();

  return objpipe::new_callback<emit_type>(
      [self, tr_begin, tr_end, group_filter, tag_filter, metric_filter](auto& cb) {
        std::vector<std::shared_ptr<const tsdata_xdr>> xdr_list;

        std::shared_ptr<const tsdata_xdr> ptr = self->read_();
        while (ptr != nullptr) {
          if ((!tr_begin.has_value() || ptr->ts() >= *tr_begin)
              && (!tr_end.has_value() || ptr->ts() <= *tr_end))
            xdr_list.push_back(ptr);
          ptr = ptr->get_predecessor();
        }

        std::reverse(xdr_list.begin(), xdr_list.end());
        if (!self->is_sorted()) {
          std::stable_sort(
              xdr_list.begin(), xdr_list.end(),
              [](const auto& x_ptr, const auto& y_ptr) {
                return x_ptr->ts() > y_ptr->ts();
              });
        }

        emit_type emit;
        if (self->is_distinct()) {
          for (const std::shared_ptr<const tsdata_xdr>& ptr : xdr_list) {
            std::get<0>(emit) = ptr->ts();
            auto& emit_map = std::get<1>(emit);

            std::shared_ptr<const record_array> ra_ptr = ptr->get();
            for (const record_array::value_type& ra_proxy : ra_ptr->filter(group_filter, tag_filter)) {
              for (const record_metrics::value_type& rm_proxy : *ra_proxy) {
                emit_map.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(ra_proxy.name(), rm_proxy.name()),
                    std::forward_as_tuple(rm_proxy.get()));
              }
            }

            cb(std::move(emit));
          }
        } else if (!xdr_list.empty()) { // not is_distinct()
          // Local scope to fill first element in emit.
          {
            const std::shared_ptr<const tsdata_xdr>& ptr = xdr_list.front();
            std::get<0>(emit) = ptr->ts();
            auto& emit_map = std::get<1>(emit);

            std::shared_ptr<const record_array> ra_ptr = ptr->get();
            for (const record_array::value_type& ra_proxy : ra_ptr->filter(group_filter, tag_filter)) {
              for (const record_metrics::value_type& rm_proxy : *ra_proxy) {
                emit_map.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(ra_proxy.name(), rm_proxy.name()),
                    std::forward_as_tuple(rm_proxy.get()));
              }
            }
          }

          // Any element past begin(), is emitted or merged, depending on timestamp.
          for (auto xdr_iter = std::next(xdr_list.begin()), xdr_end = xdr_list.end();
              xdr_iter != xdr_end;
              ++xdr_iter) {
            const std::shared_ptr<const tsdata_xdr>& ptr = *xdr_iter;
            if (std::get<0>(emit) != ptr->ts()) {
              cb(std::move(emit));
              std::get<0>(emit) = ptr->ts();
              std::get<1>(emit).clear();
            }
            auto& emit_map = std::get<1>(emit);

            std::shared_ptr<const record_array> ra_ptr = ptr->get();
            for (const record_array::value_type& ra_proxy : ra_ptr->filter(group_filter, tag_filter)) {
              for (const record_metrics::value_type& rm_proxy : *ra_proxy) {
#if __cplusplus >= 201703
                emit_map.insert_or_assign(
                    std::forward_as_tuple(ra_proxy.name(), rm_proxy.name()),
                    rm_proxy.get());
#else
                auto emplace_result = emit_map.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(ra_proxy.name(), rm_proxy.name()),
                    std::forward_as_tuple(rm_proxy.get()));
                if (std::get<1>(emplace_result)) // Already present. Override with new value.
                  std::get<0>(emplace_result)->second = rm_proxy.get();
#endif
              }
            }
          }

          // Since above never emits the entry it works on,
          // the emit value will contain un-emitted data.
          // Emit that now.
          cb(std::move(emit));
        }
      });
}

auto tsdata_v2_list::emit_time(
    std::optional<time_point> tr_begin, std::optional<time_point> tr_end) const
-> objpipe::reader<time_point> {
  return objpipe::of(shared_from_this())
      .transform(
          [tr_begin, tr_end](std::shared_ptr<const tsdata_v2_list> list) {
            std::vector<time_point> xdr_list;

            std::shared_ptr<const tsdata_xdr> ptr = list->read_();
            while (ptr != nullptr) {
              if ((!tr_begin.has_value() || ptr->ts() >= *tr_begin)
                  && (!tr_end.has_value() || ptr->ts() <= *tr_end))
                xdr_list.push_back(ptr->ts());
              ptr = ptr->get_predecessor();
            }

            std::reverse(xdr_list.begin(), xdr_list.end());
            if (!list->is_sorted()) {
              std::sort(xdr_list.begin(), xdr_list.end());
            }

            if (!list->is_distinct()) {
              std::unique(xdr_list.begin(), xdr_list.end());
            }

            return xdr_list;
          })
      .iterate();
}


}}} /* namespace monsoon::history::v2 */
