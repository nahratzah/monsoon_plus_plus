#include "encdec.h"
#include "../raw_file_segment_reader.h"
#include "../raw_file_segment_writer.h"
#include <monsoon/overload.h>
#include <monsoon/xdr/xdr_stream.h>
#include <monsoon/io/ptr_stream.h>
#include <monsoon/io/gzip_stream.h>
#include <monsoon/history/dir/hdir_exception.h>
#include <stdexcept>
#include <functional>
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
    const strval_dictionary& dict) {
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
      return metric_value(dict[in.get_uint32()]);
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
    const metric_value& value, strval_dictionary& dict) {
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
            if (v < static_cast<metric_value::unsigned_type>(std::numeric_limits<metric_value::signed_type>::max())) {
              out.put_uint32(static_cast<std::uint32_t>(metrickind::INT));
              out.put_int64(static_cast<std::int64_t>(v));
            } else { // Can't be represented in signed type.
              out.put_uint32(static_cast<std::uint32_t>(metrickind::FLOAT));
              out.put_flt64(v);
            }
          },
          [&out](const metric_value::fp_type& v) {
            out.put_uint32(static_cast<std::uint32_t>(metrickind::FLOAT));
            out.put_flt64(v);
          },
          [&out, &dict](const std::string_view& v) {
            out.put_uint32(static_cast<std::uint32_t>(metrickind::STRING));
            out.put_uint32(dict[v]);
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

auto decode_record_metrics(xdr::xdr_istream& in, const dictionary_delta& dict)
-> std::shared_ptr<time_series_value::metric_map> {
  return std::make_shared<time_series_value::metric_map>(
      in.get_collection<time_series_value::metric_map>(
          [&dict](xdr::xdr_istream& in) {
            const std::uint32_t path_ref = in.get_uint32();
            return std::make_pair(
                metric_name(dict.pdd()[path_ref]),
                decode_metric_value(in, dict.sdd()));
          }));
}

file_segment_ptr encode_record_metrics(encdec_writer& out,
    const time_series_value::metric_map& metrics,
    dictionary_delta& dict) {
  auto xdr = out.begin();
  xdr.put_collection(
      [&dict](xdr::xdr_ostream& out, const auto& entry) {
        out.put_uint32(dict.pdd()[std::get<0>(entry)]);
        encode_metric_value(out, std::get<1>(entry), dict.sdd());
      },
      metrics.begin(), metrics.end());
  xdr.close();
  return xdr.ptr();
}

auto decode_record_array(xdr::xdr_istream& in, const encdec_ctx& ctx,
    const dictionary_delta& dict)
-> std::shared_ptr<tsdata_list::record_array> {
  using namespace std::placeholders;

  auto result = std::make_shared<tsdata_list::record_array>();

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
            std::inserter(*result, result->end()),
            [&dict, &ctx](const auto& group_tuple) {
              group_name key = group_name(
                  dict.pdd()[std::get<0>(group_tuple)],
                  dict.tdd()[std::get<1>(group_tuple)]);
              auto fs = file_segment<time_series_value::metric_map>(
                  ctx,
                  std::get<2>(group_tuple),
                  std::bind(&decode_record_metrics, _1, std::cref(dict)));
              return std::make_pair(std::move(key), std::move(fs));
            });
      });
  return result;
}

file_segment_ptr encode_record_array(encdec_writer& out,
    const time_series::tsv_set& groups, dictionary_delta& dict) {
  std::unordered_map<std::uint32_t, std::unordered_map<std::uint32_t, file_segment_ptr>>
      mapping;
  std::for_each(
      groups.begin(), groups.end(),
      [&out, &mapping, &dict](const time_series_value& entry) {
        auto path_ref = dict.pdd()[entry.get_name().get_path()];
        auto tags_ref = dict.tdd()[entry.get_name().get_tags()];
        mapping[path_ref].emplace(
            tags_ref,
            encode_record_metrics(out, entry.get_metrics(), dict));
      });

  auto xdr = out.begin();
  xdr.put_collection(
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
  xdr.close();
  return xdr.ptr();
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

auto encode_tsdata(encdec_writer& writer, const time_series& ts,
    dictionary_delta dict, std::optional<file_segment_ptr> pred)
-> file_segment_ptr {
  file_segment_ptr records_ptr =
      encode_record_array(writer, ts.get_data(), dict);

  std::optional<file_segment_ptr> dict_ptr;
  if (dict.update_pending()) {
    auto xdr = writer.begin();
    dict.encode_update(xdr);
    xdr.close();
    dict_ptr = xdr.ptr();
  }

  auto xdr = writer.begin(false);
  encode_timestamp(xdr, ts.get_time());
  xdr.put_optional(&encode_file_segment, pred);
  xdr.put_optional(&encode_file_segment, dict_ptr);
  encode_file_segment(xdr, records_ptr);
  xdr.put_uint32(0u); // reserved
  xdr.close();
  return xdr.ptr();
}


tsdata_list::~tsdata_list() noexcept {}

auto tsdata_list::dictionary() const -> std::shared_ptr<dictionary_delta> {
  std::stack<file_segment_ptr> dd_stack;
  if (dd_.has_value()) dd_stack.push(dd_.value());

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
    xdr.close();
    cached_pred_ = result;
  }
  return result;
}

auto tsdata_list::records(const dictionary_delta& dict) const
-> std::shared_ptr<tsdata_list::record_array> {
  const auto lck = std::lock_guard<std::mutex>(lock_);

  std::shared_ptr<tsdata_list::record_array> result = cached_records_.lock();
  if (result == nullptr) {
    auto xdr = ctx_.new_reader(records_);
    result = decode_record_array(xdr, ctx_, dict);
    xdr.close();
    cached_records_ = result;
  }
  return result;
}

auto decode_file_data_tables_block(xdr::xdr_istream& in, const encdec_ctx& ctx)
-> file_data_tables_block {
  using namespace std::placeholders;

  auto timestamp = decode_timestamp_delta(in);
  auto dict_ptr = decode_file_segment(in);
  auto tables_data_ptr = decode_file_segment(in);

  auto xdr = ctx.new_reader(dict_ptr);
  std::shared_ptr<const dictionary_delta> dict;
  {
    auto tmp_dict = std::make_shared<dictionary_delta>();
    tmp_dict->decode_update(xdr);
    dict = std::move(tmp_dict);
    if (!xdr.at_end()) throw dirhistory_exception("xdr data remaining");
    xdr.close();
  }

  return std::make_tuple(
      std::move(timestamp),
      file_segment<tables>(
          ctx,
          tables_data_ptr,
          std::bind(&decode_tables, _1, ctx, std::move(dict))));
}

monsoon_dirhistory_local_
auto decode_file_data_tables(xdr::xdr_istream& in, const encdec_ctx& ctx)
-> std::shared_ptr<file_data_tables> {
  using namespace std::placeholders;

  return std::make_shared<file_data_tables>(
      in.get_collection<file_data_tables>(
          std::bind(&decode_file_data_tables_block, _1, std::cref(ctx))));
}

auto decode_tables(xdr::xdr_istream& in,
    const encdec_ctx& ctx,
    const std::shared_ptr<const dictionary_delta>& dict) -> std::shared_ptr<tables> {
  using namespace std::placeholders;

  auto result = std::make_shared<tables>();

  in.accept_collection(
      [](xdr::xdr_istream& in) {
        auto path_ref = in.get_uint32();
        return std::make_tuple(
            path_ref,
            in.template get_collection<std::vector<std::tuple<std::uint32_t, file_segment_ptr>>>(
                [](xdr::xdr_istream& in) {
                  auto tag_ref = in.get_uint32();
                  return std::make_tuple(tag_ref, decode_file_segment(in));
                }));
      },
      [&dict, &result, &ctx](auto&& v) {
        const simple_group path = dict->pdd()[std::get<0>(v)];

        std::transform(std::get<1>(v).begin(), std::get<1>(v).end(),
            std::inserter(*result, result->end()),
            [&path, &dict, &ctx](const auto& tag_map_entry) {
              return std::make_pair(
                  group_name(path, dict->tdd()[std::get<0>(tag_map_entry)]),
                  file_segment<group_table>(
                      ctx,
                      std::get<1>(tag_map_entry),
                      std::bind(&decode_group_table, _1, ctx, dict)));
            });
      });
  return result;
}

void encode_tables(xdr::xdr_ostream& out,
    const std::unordered_map<group_name, file_segment_ptr>& groups,
    dictionary_delta& dict) {
  // Recreate to-be-written structure in memory.
  std::unordered_map<std::uint32_t, std::unordered_map<std::uint32_t, file_segment_ptr>>
      tmp;
  std::for_each(groups.begin(), groups.end(),
      [&tmp, &dict](const auto& group) {
        const auto path_ref = dict.pdd()[std::get<0>(group).get_path()];
        const auto tag_ref = dict.tdd()[std::get<0>(group).get_tags()];
        const auto& ptr = std::get<1>(group);

        tmp[path_ref].emplace(tag_ref, ptr);
      });

  // Write the previously computed structure.
  out.put_collection(
      [](xdr::xdr_ostream& out, const auto& path_map_entry) {
        out.put_uint32(std::get<0>(path_map_entry));
        out.put_collection(
            [](xdr::xdr_ostream& out, const auto& tag_map_entry) {
              out.put_uint32(std::get<0>(tag_map_entry));
              encode_file_segment(out, std::get<1>(tag_map_entry));
            },
            std::get<1>(path_map_entry).begin(),
            std::get<1>(path_map_entry).end());
      },
      tmp.begin(),
      tmp.end());
}

auto decode_group_table(xdr::xdr_istream& in, const encdec_ctx& ctx,
    const std::shared_ptr<const dictionary_delta>& dict)
-> std::shared_ptr<group_table> {
  using namespace std::placeholders;
  using metric_map_type = std::tuple_element<1, group_table>::type;

  auto presence = decode_bitset(in);
  auto metric_map = in.get_collection<metric_map_type>(
      [&dict, &ctx](xdr::xdr_istream& in) {
        metric_name name = dict->pdd()[in.get_uint32()];
        return std::make_tuple(
            std::move(name),
            file_segment<metric_table>(
                ctx,
                decode_file_segment(in),
                std::bind(&decode_metric_table, _1, std::shared_ptr<const strval_dictionary>(dict, &dict->sdd()))));
      });

  return std::make_shared<group_table>(std::move(presence), std::move(metric_map));
}

void encode_group_table(xdr::xdr_ostream& out,
    const std::vector<bool>& presence,
    std::unordered_map<metric_name, file_segment_ptr>& metrics_map,
    dictionary_delta& dict) {
  encode_bitset(out, presence);

  out.put_collection(
      [&dict](xdr::xdr_ostream& out, const auto& mm_entry) {
        out.put_uint32(dict.pdd()[std::get<0>(mm_entry)]);
        encode_file_segment(out, std::get<1>(mm_entry));
      },
      metrics_map.begin(), metrics_map.end());
}

template<typename SerFn, typename Callback>
std::vector<bool> decode_mt(xdr::xdr_istream& in, SerFn fn, Callback cb) {
  std::vector<bool> bitset = decode_bitset(in);
  std::vector<bool>::const_iterator bitset_iter = bitset.cbegin();
  in.accept_collection(
      fn,
      [&cb, &bitset, &bitset_iter](auto&& v) {
        bitset_iter = std::find(bitset_iter, bitset.cend(), true);
        if (bitset_iter == bitset.cend())
          return; // Silent discard too many items.

        auto index = bitset_iter - bitset.cbegin();
        assert(index >= 0);
        cb(index, std::move(v));
        ++bitset_iter;
      });
  return bitset;
}

monsoon_dirhistory_local_
std::shared_ptr<metric_table> decode_metric_table(xdr::xdr_istream& in,
    const std::shared_ptr<const strval_dictionary>& dict) {
  auto result = std::make_shared<metric_table>();
  std::vector<bool> presence;

  auto callback = [&result](std::size_t index, auto&& v) {
    if (result->size() <= index) {
      result->reserve(index + 1u);
      result->resize(index);
      result->emplace_back(std::move(v));
    } else {
      if constexpr(std::is_same_v<metric_value, std::decay_t<decltype(v)>>)
        (*result)[index] = std::move(v);
      else
        (*result)[index] = metric_value(std::move(v));
    }
  };

  auto update_presence = [&presence](const std::vector<bool>& u) {
    if (presence.size() < u.size())
      presence.resize(u.size(), false);
    std::transform(u.begin(), u.end(), presence.begin(), presence.begin(),
        [](bool u, bool p) {
          if (p && u) throw xdr::xdr_exception("presence collision");
          return p || u;
        });
  };

  { // Bools are stored in a second bitset.
    auto bool_presence = decode_bitset(in);
    auto bitset_iter = bool_presence.cbegin();
    for (bool v : decode_bitset(in)) {
      bitset_iter = std::find(bitset_iter, bool_presence.cend(), true);
      if (bitset_iter == bool_presence.cend()) break; // Silent discard too many items.
      auto index = bitset_iter - bool_presence.cbegin();
      assert(index >= 0);
      std::invoke(callback, index, v);
      ++bitset_iter;
    }
    update_presence(std::move(bool_presence));
  }
  update_presence(decode_mt(in, &xdr::xdr_istream::get_int16, callback));
  update_presence(decode_mt(in, &xdr::xdr_istream::get_int32, callback));
  update_presence(decode_mt(in, &xdr::xdr_istream::get_int64, callback));
  update_presence(decode_mt(in, &xdr::xdr_istream::get_flt64, callback));
  update_presence(decode_mt(in,
          [&dict](xdr::xdr_istream& in) {
            return (*dict)[in.get_uint32()];
          },
          callback));
  update_presence(decode_mt(in, &decode_histogram, callback));
  /* empty handling */ {
    const std::vector<bool> empty_bitset = decode_bitset(in);
    update_presence(empty_bitset);

    if (result->size() < empty_bitset.size()) result->resize(empty_bitset.size());
    for (auto iter = empty_bitset.cbegin(); iter != empty_bitset.cend(); ++iter) {
      if (*iter) {
        const auto index = iter - empty_bitset.cbegin();
        (*result)[index] = metric_value(); // Assigns the optional.
      }
    }
  }
  update_presence(decode_mt(in,
          [&dict](xdr::xdr_istream& in) {
            return decode_metric_value(in, *dict);
          },
          callback));

  return result;
}

template<typename T> struct monsoon_dirhistory_local_ mt {
  std::vector<bool> presence;
  std::vector<T> value;

  template<typename SerFn>
  void encode(xdr::xdr_ostream& out, SerFn fn) {
    encode_bitset(out, presence);
    out.put_collection(fn, value.begin(), value.end());
  }
};

template<> struct monsoon_dirhistory_local_ mt<bool> {
  std::vector<bool> presence;
  std::vector<bool> value;

  void encode(xdr::xdr_ostream& out) {
    encode_bitset(out, presence);
    encode_bitset(out, value);
  }
};

monsoon_dirhistory_local_
void write_metric_table(xdr::xdr_ostream& out,
    const std::vector<std::optional<metric_value>>& metrics,
    strval_dictionary& dict) {
  using namespace std::placeholders;

  mt<bool> mt_bool{ std::vector<bool>(metrics.size(), false), {} };
  mt<std::int16_t> mt_16bit{ std::vector<bool>(metrics.size(), false), {} };
  mt<std::int32_t> mt_32bit{ std::vector<bool>(metrics.size(), false), {} };
  mt<std::int64_t> mt_64bit{ std::vector<bool>(metrics.size(), false), {} };
  mt<metric_value::fp_type> mt_dbl{ std::vector<bool>(metrics.size(), false), {} };
  mt<std::string_view> mt_str{ std::vector<bool>(metrics.size(), false), {} };
  mt<std::reference_wrapper<const histogram>> mt_hist{ std::vector<bool>(metrics.size(), false), {} };
  std::vector<bool> mt_empty = std::vector<bool>(metrics.size(), false);
  mt<std::reference_wrapper<const metric_value>> mt_other{ std::vector<bool>(metrics.size(), false), {} };

  for (auto iter = metrics.cbegin(); iter != metrics.cend(); ++iter) {
    const auto index = iter - metrics.cbegin();
    if (iter->has_value()) {
      const metric_value& mv = iter->value();
      visit(
          overload(
              [&](const auto& v) { // Fallback
                mt_other.presence[index] = true;
                mt_other.value.emplace_back(v);
              },
              [&](const metric_value::empty&) {
                mt_empty[index] = true;
              },
              [&](const bool& b) {
                mt_bool.presence[index] = true;
                mt_bool.value.push_back(b);
              },
              [&](const metric_value::signed_type& v) {
                if (v >= std::numeric_limits<std::int16_t>::min()
                    && v <= std::numeric_limits<std::int16_t>::max()) {
                  mt_16bit.presence[index] = true;
                  mt_16bit.value.push_back(v);
                } else if (v >= std::numeric_limits<std::int32_t>::min()
                    && v <= std::numeric_limits<std::int32_t>::max()) {
                  mt_32bit.presence[index] = true;
                  mt_32bit.value.push_back(v);
                } else if (v >= std::numeric_limits<std::int64_t>::min()
                    && v <= std::numeric_limits<std::int64_t>::max()) {
                  mt_64bit.presence[index] = true;
                  mt_64bit.value.push_back(v);
                } else {
                  assert(false); // Unreachable.
                }
              },
              [&](const metric_value::unsigned_type& v) {
                if (v <= static_cast<metric_value::unsigned_type>(std::numeric_limits<std::int16_t>::max())) {
                  mt_16bit.presence[index] = true;
                  mt_16bit.value.push_back(v);
                } else if (v <= static_cast<metric_value::unsigned_type>(std::numeric_limits<std::int32_t>::max())) {
                  mt_32bit.presence[index] = true;
                  mt_32bit.value.push_back(v);
                } else if (v <= static_cast<metric_value::unsigned_type>(std::numeric_limits<std::int64_t>::max())) {
                  mt_64bit.presence[index] = true;
                  mt_64bit.value.push_back(v);
                } else { // Too large to represent as integer, store as floating point.
                  mt_dbl.presence[index] = true;
                  mt_dbl.value.push_back(v);
                }
              },
              [&](const metric_value::fp_type& v) {
                mt_dbl.presence[index] = true;
                mt_dbl.value.push_back(v);
              },
              [&](const std::string_view& v) {
                mt_str.presence[index] = true;
                mt_str.value.emplace_back(v);
              },
              [&](const histogram& v) {
                mt_hist.presence[index] = true;
                mt_hist.value.emplace_back(v);
              }
              ),
          mv.get());
    }
  }

  mt_bool.encode(out);
  mt_16bit.encode(out, &xdr::xdr_ostream::put_int16);
  mt_32bit.encode(out, &xdr::xdr_ostream::put_int32);
  mt_64bit.encode(out, &xdr::xdr_ostream::put_int64);
  mt_dbl.encode(out, &xdr::xdr_ostream::put_flt64);
  mt_str.encode(out,
      [&dict](xdr::xdr_ostream& out, const auto& v) {
        out.put_uint32(dict[v]);
      });
  mt_hist.encode(out, &encode_histogram);
  encode_bitset(out, mt_empty);
  mt_other.encode(out,
      std::bind(&encode_metric_value, _1, _2, std::ref(dict)));
}


encdec_ctx::encdec_ctx() noexcept {}

encdec_ctx::encdec_ctx(const encdec_ctx& o)
: fd_(o.fd_),
  hdr_flags_(o.hdr_flags_)
{}

encdec_ctx::encdec_ctx(encdec_ctx&& o) noexcept
: fd_(std::move(o.fd_)),
  hdr_flags_(std::move(o.hdr_flags_))
{}

auto encdec_ctx::operator=(const encdec_ctx& o) -> encdec_ctx& {
  fd_ = o.fd_;
  hdr_flags_ = o.hdr_flags_;
  return *this;
}

auto encdec_ctx::operator=(encdec_ctx&& o) noexcept -> encdec_ctx& {
  fd_ = std::move(o.fd_);
  hdr_flags_ = std::move(o.hdr_flags_);
  return *this;
}

encdec_ctx::encdec_ctx(std::shared_ptr<io::fd> fd, std::uint32_t hdr_flags)
  noexcept
: fd_(std::move(fd)),
  hdr_flags_(hdr_flags)
{}

encdec_ctx::~encdec_ctx() noexcept {}

auto encdec_ctx::new_reader(const file_segment_ptr& ptr, bool compression)
  const
-> xdr::xdr_stream_reader<io::ptr_stream_reader> {
  auto rd = io::make_ptr_reader<raw_file_segment_reader>(
      *fd_, ptr.offset(),
      ptr.size());
  if (compression) rd = decompress(std::move(rd), true);
  return xdr::xdr_stream_reader<io::ptr_stream_reader>(std::move(rd));
}

auto encdec_ctx::decompress(io::ptr_stream_reader&& rd, bool validate) const
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

auto encdec_ctx::compress(io::ptr_stream_writer&& wr) const
-> std::unique_ptr<io::stream_writer> {
  switch (compression()) {
    default:
    case compression_type::LZO_1X1: // XXX implement reader
    case compression_type::SNAPPY: // XXX implement reader
      throw std::logic_error("Unsupported compression");
    case compression_type::NONE:
      return std::move(wr).get();
    case compression_type::GZIP:
      return io::new_gzip_compression(std::move(wr));
  }
}


tsfile_header::tsfile_header(xdr::xdr_istream& in,
    std::shared_ptr<io::fd> fd) {
  using namespace std::placeholders;

  first_ = decode_timestamp(in);
  last_ = decode_timestamp(in);
  flags_ = in.get_uint32();
  reserved_ = in.get_uint32();
  file_size_ = in.get_uint64();

  auto fdt_ptr = decode_file_segment(in);
  switch (flags_ & header_flags::KIND_MASK) {
    default:
      throw xdr::xdr_exception("file kind not recognized");
    case header_flags::KIND_LIST:
      fdt_ = file_segment<tsdata_list>(
          encdec_ctx(fd, flags_),
          fdt_ptr,
          std::bind(&decode_tsdata, _1, encdec_ctx(fd, flags_)),
          false);
      break;
    case header_flags::KIND_TABLES:
      fdt_ = file_segment<file_data_tables>(
          encdec_ctx(fd, flags_),
          fdt_ptr,
          std::bind(&decode_file_data_tables, _1, encdec_ctx(fd, flags_)));
      break;
  }
}

tsfile_header::~tsfile_header() noexcept {}


file_segment_ptr encdec_writer::commit(const std::uint8_t* buf,
    std::size_t len, bool compress) {
  const auto fd_ptr = ctx_.fd();

  io::fd::size_type dlen, slen;
  auto wr = io::make_ptr_writer<raw_file_segment_writer>(*fd_ptr, off_, &dlen, &slen);
  if (compress) wr = ctx_.compress(std::move(wr));

  while (len > 0) {
    const auto wlen = wr.write(buf, len);
    len -= wlen;
    buf += wlen;
  }

  wr.close();
  auto result = file_segment_ptr(off_, dlen);
  off_ += slen;
  return result;
}

encdec_writer::xdr_writer::~xdr_writer() noexcept {}

void encdec_writer::xdr_writer::close() {
  assert(ecw_ != nullptr);
  assert(!closed_);
  ptr_ = ecw_->commit(buffer_.data(), buffer_.size(), compress_);
  closed_ = true;
}

file_segment_ptr encdec_writer::xdr_writer::ptr() const noexcept {
  assert(ecw_ != nullptr);
  assert(closed_);
  return ptr_;
}

void encdec_writer::xdr_writer::put_raw_bytes(const void* buf,
    std::size_t len) {
  const std::uint8_t* buf_begin = reinterpret_cast<const std::uint8_t*>(buf);
  const std::uint8_t* buf_end = buf_begin + len;
  buffer_.insert(buffer_.end(), buf_begin, buf_end);
}


}}} /* namespace monsoon::history::v2 */
