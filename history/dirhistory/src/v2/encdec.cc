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
#include "group_table.h"

namespace monsoon {
namespace history {
namespace v2 {


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

file_segment_ptr decode_file_segment(xdr::xdr_istream& in) {
  return file_segment_ptr::from_xdr(in);
}

void encode_file_segment(xdr::xdr_ostream& out, const file_segment_ptr& fsp) {
  fsp.encode(out);
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
  return group_table::from_xdr(nullptr, in, std::const_pointer_cast<dictionary_delta>(dict), ctx);
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
