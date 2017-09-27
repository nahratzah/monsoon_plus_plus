#include "encdec.h"
#include "../overload.h"
#include "../raw_file_segment_reader.h"
#include <monsoon/xdr/xdr_stream.h>
#include <monsoon/io/gzip_stream.h>
#include <monsoon/history/dir/hdir_exception.h>
#include <stdexcept>
#include <algorithm>
#include <stack>
#include <cstring>

namespace monsoon {
namespace history {
namespace v2 {


enum class metrickind : std::uint32_t {
  BOOL = 0,
  INT = 1,
  FLOAT = 2,
  STRING = 3,
  HISTOGRAM = 4,
  EMPTY = 0x7fffffff
};

time_point decode_timestamp(xdr::xdr_istream& in) {
  return time_point(in.get_int64());
}

void encode_timestamp(xdr::xdr_ostream& out, time_point tp) {
  out.put_int64(tp.millis_since_posix_epoch());
}

std::vector<time_point> decode_timestamp_delta(xdr::xdr_istream& in) {
  std::vector<time_point> result;
  result.push_back(time_point(in.get_int64()));
  in.accept_collection(
      [](xdr::xdr_istream& in) {
        return time_point::duration(in.get_int32());
      },
      [&result](time_point::duration delta) {
        result.push_back(result.back() + delta);
      });
  return result;
}

void encode_timestamp_delta(xdr::xdr_ostream& out,
    std::vector<time_point>&& tp_set) {
  if (tp_set.empty())
    throw std::invalid_argument("empty time_point collection");
  std::sort(tp_set.begin(), tp_set.end());

  auto pred = tp_set.front().millis_since_posix_epoch();
  out.put_int64(pred);
  out.put_collection(
      [&pred](xdr::xdr_ostream& out, const time_point& tp) {
        const std::int64_t tp_millis = tp.millis_since_posix_epoch();
        const std::int64_t delta = tp_millis - pred;
        pred = tp_millis;
        if (delta > 0x7fffffff || delta < -0x80000000) {
          throw std::invalid_argument("time between successive timestamps "
              "is too large");
        }
        out.put_int32(delta);
      },
      tp_set.begin() + 1, tp_set.end());
}

std::vector<bool> decode_bitset(xdr::xdr_istream& in) {
  std::vector<bool> result;
  bool current = true;
  in.accept_collection(
      [](xdr::xdr_istream& in) {
        return in.get_uint16();
      },
      [&result, &current](std::uint16_t count) {
        std::fill_n(std::back_inserter(result), count, current);
        current = !current;
      });
  return result;
}

void encode_bitset(xdr::xdr_ostream& out, const std::vector<bool>& bits) {
  using namespace std::placeholders;

  bool current = true;
  std::vector<std::uint16_t> counters;

  auto bit_iter = bits.begin();
  while (bit_iter != bits.end()) {
    const auto end = std::find(bit_iter, bits.end(), !current);
    auto count = end - bit_iter;

    while (count > 0x7fff) {
      counters.push_back(0x7fff);
      counters.push_back(0);
      count -= 0x7fff;
    }
    counters.push_back(count);

    current = !current;
    bit_iter = end;
  }

  out.put_collection(std::bind(&xdr::xdr_ostream::put_uint16, _1, _2),
      counters.begin(), counters.end());
}

histogram decode_histogram(monsoon::xdr::xdr_istream& in) {
  histogram result;
  in.accept_collection(
      [](monsoon::xdr::xdr_istream& in) {
        const double lo = in.get_flt64();
        const double hi = in.get_flt64();
        const double count = in.get_flt64();
        return std::make_tuple(histogram::range(lo, hi), count);
      },
      [&result](const std::tuple<histogram::range, double>& v) {
        result.add(std::get<0>(v), std::get<1>(v));
      });
  return result;
}

void encode_histogram(monsoon::xdr::xdr_ostream& out, const histogram& hist) {
  const auto& data = hist.data();
  out.put_collection(
      [](monsoon::xdr::xdr_ostream& out, const auto& hist_elem) {
        out.put_flt64(std::get<0>(hist_elem).low());
        out.put_flt64(std::get<0>(hist_elem).high());
        out.put_flt64(std::get<1>(hist_elem));
      },
      data.begin(), data.end());
}

metric_value decode_metric_value(monsoon::xdr::xdr_istream& in,
    const dictionary<std::string>& dict) {
  switch (in.get_uint32()) {
    default:
      throw xdr::xdr_exception();
    case static_cast<std::uint32_t>(metrickind::BOOL):
      return metric_value(in.get_bool());
      break;
    case static_cast<std::uint32_t>(metrickind::INT):
      return metric_value(in.get_int64());
      break;
    case static_cast<std::uint32_t>(metrickind::FLOAT):
      return metric_value(in.get_flt64());
      break;
    case static_cast<std::uint32_t>(metrickind::STRING):
      return metric_value(dict.decode(in.get_uint32()));
      break;
    case static_cast<std::uint32_t>(metrickind::HISTOGRAM):
      return metric_value(decode_histogram(in));
      break;
    case static_cast<std::uint32_t>(metrickind::EMPTY):
      return metric_value();
      break;
  }
}

void encode_metric_value(monsoon::xdr::xdr_ostream& out,
    const metric_value& value, dictionary<std::string>& dict) {
  visit(
      overload(
          [&out](const metric_value::empty&) {
            out.put_uint32(static_cast<std::uint32_t>(metrickind::EMPTY));
          },
          [&out](bool b) {
            out.put_uint32(static_cast<std::uint32_t>(metrickind::BOOL));
            out.put_bool(b);
          },
          [&out](const metric_value::signed_type& v) {
            out.put_uint32(static_cast<std::uint32_t>(metrickind::INT));
            out.put_int64(v);
          },
          [&out](const metric_value::unsigned_type& v) {
            out.put_uint32(static_cast<std::uint32_t>(metrickind::INT));
            out.put_int64(static_cast<std::int64_t>(v));
          },
          [&out](const metric_value::fp_type& v) {
            out.put_uint32(static_cast<std::uint32_t>(metrickind::FLOAT));
            out.put_flt64(v);
          },
          [&out, &dict](const std::string& v) {
            out.put_uint32(static_cast<std::uint32_t>(metrickind::STRING));
            out.put_uint32(dict.encode(v));
          },
          [&out](const histogram& v) {
            out.put_uint32(static_cast<std::uint32_t>(metrickind::HISTOGRAM));
            encode_histogram(out, v);
          }),
      value.get());
}

file_segment_ptr decode_file_segment(xdr::xdr_istream& in) {
  const std::uint64_t offset = in.get_uint64();
  const std::uint64_t len = in.get_uint64();
  return file_segment_ptr(offset, len);
}

void encode_file_segment(xdr::xdr_ostream& out, const file_segment_ptr& fsp) {
  out.put_uint64(fsp.offset());
  out.put_uint64(fsp.size());
}

time_series_value::metric_map decode_record_metrics(xdr::xdr_istream& in,
    const dictionary_delta& dict) {
  return in.get_collection<time_series_value::metric_map>(
      [&dict](xdr::xdr_istream& in) {
        const std::uint32_t path_ref = in.get_uint32();
        return std::make_pair(
            metric_name(dict.pdd.decode(path_ref)),
            decode_metric_value(in, dict.sdd));
      });
}

void encode_record_metrics(xdr::xdr_ostream& out,
    const time_series_value::metric_map& metrics,
    dictionary_delta& dict) {
  out.put_collection(
      [&dict](xdr::xdr_ostream& out, const auto& entry) {
        out.put_uint32(dict.pdd.encode(std::get<0>(entry).get_path()));
        encode_metric_value(out, std::get<1>(entry), dict.sdd);
      },
      metrics.begin(), metrics.end());
}

auto decode_record_array(xdr::xdr_istream& in, const encdec_ctx& ctx,
    const dictionary_delta& dict)
-> tsdata_list::record_array {
  using namespace std::placeholders;

  std::unordered_map<group_name, file_segment<time_series_value::metric_map>>
      result;

  in.accept_collection(
      [](xdr::xdr_istream& in) {
        const auto path_ref = in.get_uint32();
        return in.get_collection<std::vector<std::tuple<std::uint32_t, std::uint32_t, file_segment_ptr>>>(
            [path_ref](xdr::xdr_istream& in) {
              const auto tag_ref = in.get_uint32();
              return std::make_tuple(path_ref, tag_ref, decode_file_segment(in));
            });
      },
      [&dict, &ctx, &result](auto&& group_list) {
        std::transform(
            group_list.begin(), group_list.end(),
            std::inserter(result, result.end()),
            [&dict, &ctx](const auto& group_tuple) {
              group_name key = group_name(
                  simple_group(dict.pdd.decode(std::get<0>(group_tuple))),
                  dict.tdd.decode(std::get<1>(group_tuple)));
              auto fs = file_segment<time_series_value::metric_map>(
                  ctx,
                  std::get<2>(group_tuple),
                  std::bind(&decode_record_metrics, _1, dict));
              return std::make_pair(std::move(key), std::move(fs));
            });
      });
  return result;
}

void encode_record_array(xdr::xdr_ostream& out,
    const std::vector<std::pair<group_name, file_segment_ptr>>& groups,
    dictionary_delta& dict) {
  std::unordered_map<std::uint32_t, std::unordered_map<std::uint32_t, file_segment_ptr>>
      mapping;
  std::for_each(
      groups.begin(), groups.end(),
      [&mapping, &dict](const std::pair<group_name, file_segment_ptr>& entry) {
        auto path_ref = dict.pdd.encode(entry.first.get_path().get_path());
        auto tags_ref = dict.tdd.encode(entry.first.get_tags());
        mapping[path_ref].emplace(tags_ref, entry.second);
      });

  out.put_collection(
      [](xdr::xdr_ostream& out, const auto& path_entry) {
        out.put_uint32(path_entry.first);
        out.put_collection(
            [](xdr::xdr_ostream& out, const auto& tag_entry) {
              out.put_uint32(tag_entry.first);
              encode_file_segment(out, tag_entry.second);
            },
            path_entry.second.begin(), path_entry.second.end());
      },
      mapping.begin(), mapping.end());
}

void dictionary_delta::decode_update(xdr::xdr_istream& in) {
  using namespace std::placeholders;

  sdd.decode_update(in, [](xdr::xdr_istream& in) { return in.get_string(); });
  pdd.decode_update(
      in,
      [this](xdr::xdr_istream& in) {
        return in.template get_collection<std::vector<std::string>>(
            [this](xdr::xdr_istream& in) {
              return sdd.decode(in.get_uint32());
            });
      });
  tdd.decode_update(
      in,
      [this](xdr::xdr_istream& in) {
        auto keys = in.template get_collection<std::vector<std::uint32_t>>(
            std::bind(&xdr::xdr_istream::get_uint32, _1));
        auto values = in.template get_collection<std::vector<metric_value>>(
            [this](xdr::xdr_istream& in) {
              return decode_metric_value(in, sdd);
            });
        if (keys.size() != values.size())
          throw xdr::xdr_exception("tag dictionary length mismatch");

        tags::map_type tag_map;
        std::transform(
            keys.begin(), keys.end(), std::make_move_iterator(values.begin()),
            std::inserter(tag_map, tag_map.end()),
            [this](std::uint32_t str_ref, metric_value&& mv) {
              return std::make_pair(sdd.decode(str_ref), std::move(mv));
            });
        return tags(std::move(tag_map));
      });
}

void dictionary_delta::encode_update(xdr::xdr_ostream& out) {
  // Pre-compute everything but sdd, since their computation will modify sdd.
  // They have to be written out after sdd though.
  xdr::xdr_bytevector_ostream<> pre_computed;

  pdd.encode_update(
      pre_computed,
      [this](xdr::xdr_ostream& out, const std::vector<std::string>& v) {
        out.put_collection(
            [this](xdr::xdr_ostream& out, const std::string& s) {
              out.put_uint32(this->sdd.encode(s));
            },
            v.begin(), v.end());
      });
  tdd.encode_update(
      pre_computed,
      [this](xdr::xdr_ostream& out, const tags& v) {
        std::vector<const metric_value*> values;

        const auto& tag_map = v.get_map();
        values.reserve(tag_map.size());

        // First pass: encode metric name keys (using dictionary).
        out.put_collection(
            [&values, this](xdr::xdr_ostream& out, const auto& entry) {
              values.push_back(&entry.second); // Use in second pass.
              out.put_uint32(sdd.encode(entry.first));
            },
            tag_map.begin(), tag_map.end());
        // Second pass: encode metric values.
        out.put_collection(
            [this](xdr::xdr_ostream& out, const auto& v) {
              encode_metric_value(out, *v, sdd);
            },
            values.begin(), values.end());
      });

  sdd.encode_update(
      out,
      [](xdr::xdr_ostream& out, const auto& v) {
        out.put_string(v);
      });
  pre_computed.copy_to(out);
}

auto decode_tsdata(xdr::xdr_istream& in, const encdec_ctx& ctx)
-> std::shared_ptr<tsdata_list> {
  using namespace std::placeholders;

  const auto ts = decode_timestamp(in);
  const auto previous_ptr = in.get_optional(&decode_file_segment);
  const auto dict_ptr = in.get_optional(&decode_file_segment);
  const auto records_ptr = decode_file_segment(in);
  const auto reserved = in.get_uint32();

  return std::make_shared<tsdata_list>(
      ctx, ts, previous_ptr, dict_ptr, records_ptr, reserved);
}


tsdata_list::~tsdata_list() noexcept {}

auto tsdata_list::dictionary() const -> std::shared_ptr<dictionary_delta> {
  std::stack<file_segment_ptr> dd_stack;
  if (dd_) dd_stack.push(dd_.value());

  for (std::shared_ptr<tsdata_list> pred_tsdata = this->pred();
      pred_tsdata != nullptr;
      pred_tsdata = pred_tsdata->pred()) {
    auto pdd = pred_tsdata->dd_;
    if (pdd.has_value())
      dd_stack.push(std::move(pdd).value());
  }

  auto dict = std::make_shared<dictionary_delta>();
  while (!dd_stack.empty()) {
    auto xdr = ctx_.new_reader(dd_stack.top());
    dict->decode_update(xdr);
    if (!xdr.at_end()) throw dirhistory_exception("xdr data remaining");
    xdr.close();
    dd_stack.pop();
  }
  return dict;
}

auto tsdata_list::pred() const -> std::shared_ptr<tsdata_list> {
  if (!pred_.has_value()) return nullptr;
  const auto lck = std::lock_guard<std::mutex>(lock_);

  std::shared_ptr<tsdata_list> result = cached_pred_.lock();
  if (result == nullptr) {
    auto xdr = ctx_.new_reader(pred_.value(), false);
    result = decode_tsdata(xdr, ctx_);
    cached_pred_ = result;
  }
  return result;
}


auto encdec_ctx::new_reader(const file_segment_ptr& ptr, bool compression)
  const
-> xdr::xdr_stream_reader<io::ptr_stream_reader> {
  auto rd = io::make_ptr_reader<raw_file_segment_reader>(
      *fd_, ptr.offset(),
      ptr.size());
  if (compression && this->compression() != compression_type::NONE)
    rd = decompress_(std::move(rd), true);
  return xdr::xdr_stream_reader<io::ptr_stream_reader>(std::move(rd));
}

auto encdec_ctx::decompress_(io::ptr_stream_reader&& rd, bool validate) const
-> std::unique_ptr<io::stream_reader> {
  switch (compression()) {
    default:
    case compression_type::LZO_1X1: // XXX implement reader
    case compression_type::SNAPPY: // XXX implement reader
      throw std::logic_error("Unsupported compression");
    case compression_type::NONE:
      return std::move(rd).get();
    case compression_type::GZIP:
      return io::new_gzip_decompression(std::move(rd), validate);
  }
}


}}} /* namespace monsoon::history::v2 */
