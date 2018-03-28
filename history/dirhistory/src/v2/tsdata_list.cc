#include "tsdata_list.h"
#include "../raw_file_segment_writer.h"
#include <objpipe/callback.h>
#include <objpipe/of.h>

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

auto tsdata_v2_list::emit(
    std::optional<time_point> tr_begin, std::optional<time_point> tr_end,
    const path_matcher& group_filter,
    const tag_matcher& tag_filter,
    const path_matcher& metric_filter) const
-> objpipe::reader<emit_type> {
  std::shared_ptr<const tsdata_v2_list> self = shared_from_this();

  if (is_sorted()) {
    if (is_distinct()) {
      return objpipe::new_callback<emit_type>(
          [self, tr_begin, tr_end, group_filter, tag_filter, metric_filter](auto& cb) {
            self->direct_emit_cb_(tr_begin, tr_end, group_filter, tag_filter, metric_filter, cb);
          });
    } else {
      return objpipe::new_callback<emit_type>(
          [self, tr_begin, tr_end, group_filter, tag_filter, metric_filter](auto& cb) {
            self->emit_cb_sorted_with_duplicates_(tr_begin, tr_end, group_filter, tag_filter, metric_filter, cb);
          });
    }
  } else {
    return objpipe::new_callback<emit_type>(
        [self, tr_begin, tr_end, group_filter, tag_filter, metric_filter](auto& cb) {
          self->emit_cb_unsorted_(tr_begin, tr_end, group_filter, tag_filter, metric_filter, cb);
        });
  }
}

auto tsdata_v2_list::emit_time(
    std::optional<time_point> tr_begin, std::optional<time_point> tr_end) const
-> objpipe::reader<time_point> {
  std::shared_ptr<const tsdata_v2_list> self = shared_from_this();

  if (is_sorted()) {
    if (is_distinct()) {
      return objpipe::new_callback<time_point>(
          [self, tr_begin, tr_end](auto& cb) {
            self->visit(
                [&cb, &tr_begin, &tr_end](const time_point& tp, [[maybe_unused]] const auto& records) {
                  if ((!tr_begin.has_value() || *tr_begin <= tp)
                      && (!tr_end.has_value() || *tr_end >= tp))
                    cb(std::move(tp));
                });
          })
          .filter(
              [tr_begin](const time_point& tp) {
                return !tr_begin.has_value() || *tr_begin <= tp;
              })
          .filter(
              [tr_end](const time_point& tp) {
                return !tr_end.has_value() || *tr_end >= tp;
              });
    } else { // sorted, with duplicates
      return objpipe::new_callback<time_point>(
          [self, tr_begin, tr_end](auto cb) {
            std::optional<time_point> previous_tp;
            self->visit(
                [&cb, &previous_tp](const time_point& tp, [[maybe_unused]] const auto& records) {
                  if (previous_tp != tp) {
                    previous_tp = tp;
                    cb(std::move(tp));
                  }
                });
          })
          .filter(
              [tr_begin](const time_point& tp) {
                return !tr_begin.has_value() || *tr_begin <= tp;
              })
          .filter(
              [tr_end](const time_point& tp) {
                return !tr_end.has_value() || *tr_end >= tp;
              });
    }
  } else { // unsorted
    std::vector<time_point> data;
    visit(
        [&data, &tr_begin, &tr_end](const time_point& tp, [[maybe_unused]] const auto& records) {
          if ((!tr_begin.has_value() || *tr_begin <= tp)
              && (!tr_end.has_value() || *tr_end >= tp))
            data.push_back(tp);
        });
    std::sort(data.begin(), data.end());
    if (!is_distinct()) {
      data.erase(
          std::unique(data.begin(), data.end()),
          data.end());
    }
    return objpipe::of(std::move(data))
        .iterate();
  }
}


}}} /* namespace monsoon::history::v2 */
