#include "tsdata.h"
#include "../tsdata_mime.h"
#include <monsoon/overload.h>
#include <algorithm>
#include <monsoon/io/gzip_stream.h>
#include <monsoon/io/positional_stream.h>
#include <monsoon/xdr/xdr_stream.h>
#include <limits>
#include <vector>
#include <unordered_map>
#include <monsoon/xdr/xdr.h>
#include <monsoon/objpipe/callback.h>

namespace monsoon {
namespace history {
namespace v1 {


constexpr std::uint16_t tsdata_v1::MAJOR;
constexpr std::uint16_t tsdata_v1::MAX_MINOR;

tsdata_v1::tsdata_v1(io::fd&& file)
: file_(std::move(file)),
  gzipped_(io::is_gzip_file(io::positional_reader(file_)))
{
  const auto r = make_xdr_istream(false);
  const tsfile_mimeheader hdr = tsfile_mimeheader(*r);
  minor_version = hdr.minor_version;
  std::tie(tp_begin_, tp_end_) = decode_tsfile_header(*r);

  if (hdr.major_version == MAJOR && hdr.minor_version <= MAX_MINOR) {
    /* SKIP: file is acceptable */
  } else {
    throw xdr::xdr_exception();
  }
}

tsdata_v1::~tsdata_v1() noexcept {}

auto tsdata_v1::read_all() const -> std::vector<time_series> {
  std::vector<time_series> result;
  visit([&result](auto&& ts) { result.push_back(std::move(ts)); });
  return result;
}

std::shared_ptr<tsdata_v1> tsdata_v1::write_all(
    const std::string& fname,
    std::vector<time_series>&& datums, bool compress) {
  std::sort(datums.begin(), datums.end(),
      [](const time_series& x, const time_series& y) -> bool {
        return x.get_time() < y.get_time();
      });

  io::fd file = io::fd::create(fname);
  try {
    // Create xdr writer.
    std::unique_ptr<xdr::xdr_ostream> w;
    if (compress) {
      w = std::make_unique<xdr::xdr_stream_writer<io::gzip_compress_writer<io::positional_writer>>>(
          io::gzip_compress_writer<io::positional_writer>(io::positional_writer(file), 9));
    } else {
      w = std::make_unique<xdr::xdr_stream_writer<io::positional_writer>>(
          io::positional_writer(file));
    }

    // Write monsoon mime header.
    tsfile_mimeheader(MAJOR, MAX_MINOR).write(*w);

    // Write tsdata v0 header.
    time_point b, e;
    if (datums.empty()) {
      b = e = time_point::now();
    } else {
      b = datums.front().get_time();
      e = datums.back().get_time();
    }
    encode_tsfile_header(*w, std::tie(b, e));

    // Write the timeseries.
    dictionary_delta dict;
    std::for_each(datums.begin(), datums.end(),
        [&w, &dict](const time_series& ts) {
          encode_time_series(*w, ts, dict);
        });

    // Close the writer, so data gets flushed properly.
    w->close();
    file.flush(); // Sync to disk.

    return std::make_shared<tsdata_v1>(std::move(file));
  } catch (...) {
    // XXX delete file
    throw;
  }
}

auto tsdata_v1::version() const noexcept
-> std::tuple<std::uint16_t, std::uint16_t> {
  return std::make_tuple(MAJOR, minor_version);
}

bool tsdata_v1::is_writable() const noexcept {
  return file_.can_write() && !gzipped_;
}

void tsdata_v1::push_back(const time_series& ts) {
  if (gzipped_) throw std::runtime_error("not writable");

  const time_point tp = ts.get_time();
  const auto orig_size = file_.size();

  try {
    {
      auto w = xdr::xdr_stream_writer<io::positional_writer>(io::positional_writer(
              file_, orig_size));
      encode_time_series(w, ts, get_dict_());
      file_.flush();
    }

    bool update_hdr = false;
    if (tp < tp_begin_)
      std::tie(tp_begin_, update_hdr) = std::make_tuple(tp, true);
    if (tp > tp_end_)
      std::tie(tp_end_, update_hdr) = std::make_tuple(tp, true);
    if (update_hdr) {
      auto w = xdr::xdr_stream_writer<io::positional_writer>(io::positional_writer(
              file_, tsfile_mimeheader::XDR_ENCODED_LEN));
      encode_tsfile_header(w, std::tie(tp_begin_, tp_end_));
      file_.flush();
    }
  } catch (...) {
    dict_.reset(); // Since we may have modified it.
    file_.truncate(orig_size);
    throw;
  }
}

auto tsdata_v1::time() const -> std::tuple<time_point, time_point> {
  return std::make_tuple(tp_begin_, tp_end_);
}

auto tsdata_v1::new_file(io::fd&& fd, time_point tp)
-> std::shared_ptr<tsdata_v1> {
  auto w = xdr::xdr_stream_writer<io::positional_writer>(io::positional_writer(
          fd, 0u));
  tsfile_mimeheader(MAJOR, MAX_MINOR).write(w);
  encode_tsfile_header(w, std::tie(tp, tp));
  fd.flush();
  return std::make_shared<tsdata_v1>(std::move(fd));
}

auto tsdata_v1::emit(
    std::optional<time_point> tr_begin,
    std::optional<time_point> tr_end,
    const path_matcher& group_filter,
    const tag_matcher& tag_filter,
    const path_matcher& metric_filter)
    const
-> objpipe::reader<emit_type> {
  using emit_map = std::tuple_element_t<1, emit_type>;

  std::shared_ptr<const tsdata_v1> self = shared_from_this();

  return objpipe::new_callback<emit_type>(
      [self, tr_begin, tr_end, group_filter, tag_filter, metric_filter](auto cb) {
        self->visit(
            [&](auto&& ts) {
              const time_point tp = ts.get_time();
              if (tr_begin.has_value() && tp < tr_begin.value()) return;
              if (tr_end.has_value() && tp > tr_end.value()) return;

              emit_map values;
              std::for_each(ts.data().begin(), ts.data().end(),
                  [&values, &group_filter, &tag_filter, &metric_filter](auto&& tsv) {
                    if (!group_filter(tsv.get_name().get_path())) return;
                    if (!tag_filter(tsv.get_name().get_tags())) return;

                    const auto& metrics_map = tsv.get_metrics();
                    std::for_each(metrics_map.begin(), metrics_map.end(),
                        [&tsv, &values, &metric_filter](auto&& entry) {
                          if (!metric_filter(std::get<0>(entry)))
                            return;

                          values.emplace(
                              std::piecewise_construct,
                              std::tie(tsv.get_name(), std::get<0>(entry)),
                              std::tie(std::get<1>(entry)));
                        });
                  });

              cb(emit_type(tp, std::move(values)));
            });
      });
}

auto tsdata_v1::emit_time(
    std::optional<time_point> tr_begin,
    std::optional<time_point> tr_end) const
-> objpipe::reader<time_point> {
  std::shared_ptr<const tsdata_v1> self = shared_from_this();

  return objpipe::new_callback<time_point>(
      [self, tr_begin, tr_end](auto cb) {
        self->visit(
            [&](auto&& ts) {
              if ((!tr_begin.has_value() || tr_begin.value() <= ts.get_time())
                  && (!tr_end.has_value() || tr_end.value() >= ts.get_time()))
                cb(ts.get_time());
            });
      });
}

auto tsdata_v1::make_xdr_istream(bool validate) const
-> std::unique_ptr<xdr::xdr_istream> {
  if (gzipped_) {
    return std::make_unique<xdr::xdr_stream_reader<io::gzip_decompress_reader<io::positional_reader>>>(
        io::gzip_decompress_reader<io::positional_reader>(io::positional_reader(file_), validate));
  } else {
    return std::make_unique<xdr::xdr_stream_reader<io::positional_reader>>(
        io::positional_reader(file_));
  }
}

auto tsdata_v1::get_dict_() const -> const dictionary_delta& {
  fault_dict_();
  return dict_.value();
}

auto tsdata_v1::get_dict_() -> dictionary_delta& {
  fault_dict_();
  return dict_.value();
}

void tsdata_v1::fault_dict_() const {
  if (dict_.has_value()) return;

  const auto r = make_xdr_istream(true);
  auto hdr = tsfile_mimeheader(*r); // Decode and discard.
  decode_tsfile_header(*r); // Decode and discard.

  dictionary_delta dict;
  std::vector<time_series> result;
  while (!r->at_end())
    decode_time_series(*r, dict); // Decode and discard, updates dictionary.
  r->close();

  assert(!dict.update_pending());
  dict_ = std::move(dict);
}


template<typename T>
template<typename SerFn>
void dictionary<T>::decode_update(xdr::xdr_istream& in, SerFn fn) {
  in.accept_collection(
      [&fn](xdr::xdr_istream& in) {
        const std::uint32_t key = in.get_uint32();
        return std::make_tuple(key, fn(in));
      },
      [this](const auto& entry) {
        if (decode_map_.size() <= std::get<0>(entry))
          decode_map_.resize(std::get<0>(entry) + 1u);
        decode_map_[std::get<0>(entry)] = std::get<1>(entry);
        encode_map_[std::get<1>(entry)] = std::get<0>(entry);
      });
  update_start_ = decode_map_.size();
}

template<typename T>
template<typename SerFn>
void dictionary<T>::encode_update(xdr::xdr_ostream& out, SerFn fn) {
  std::uint32_t idx = update_start_;
  out.put_collection(
      [&idx, &fn, this](xdr::xdr_ostream& out, const auto& value) {
        assert(idx < decode_map_.size() && decode_map_[idx] == value);

        out.put_uint32(idx++);
        fn(out, value);
      },
      decode_map_.begin() + update_start_, decode_map_.end());

  assert(idx == decode_map_.size());
  update_start_ = decode_map_.size();
}


enum class metrickind : std::uint32_t {
  BOOL = 0,
  INT = 1,
  FLOAT = 2,
  STRING = 3,
  HISTOGRAM = 4,
  EMPTY = 0x7fffffff
};

void dictionary_delta::decode_update(xdr::xdr_istream& in) {
  sdd.decode_update(in,
      [](xdr::xdr_istream& in) {
        return in.get_string();
      });
  gdd.decode_update(in,
      [](xdr::xdr_istream& in) {
        return simple_group(decode_path(in));
      });
  mdd.decode_update(in,
      [](xdr::xdr_istream& in) {
        return metric_name(decode_path(in));
      });
  tdd.decode_update(in,
      [this](xdr::xdr_istream& in) {
        return decode_tags(in, this->sdd);
      });
}

void dictionary_delta::encode_update(xdr::xdr_ostream& out) {
  // Encode tags first, since this may trigger string updates.
  auto pre_encoded_tags = xdr::xdr_bytevector_ostream<>();
  tdd.encode_update(pre_encoded_tags,
      [this](xdr::xdr_ostream& out, const auto& v) {
        encode_tags(out, v, this->sdd);
      });

  sdd.encode_update(out,
      [](xdr::xdr_ostream& out, const auto& v) {
        out.put_string(v);
      });
  gdd.encode_update(out,
      [](xdr::xdr_ostream& out, const auto& v) {
        encode_path(out, v);
      });
  mdd.encode_update(out,
      [](xdr::xdr_ostream& out, const auto& v) {
        encode_path(out, v);
      });
  pre_encoded_tags.copy_to(out);
}


auto decode_tsfile_header(monsoon::xdr::xdr_istream& in)
-> std::tuple<time_point, time_point> {
  time_point begin = decode_timestamp(in);
  time_point end =decode_timestamp(in);
  return std::make_tuple(std::move(begin), std::move(end));
}

void encode_tsfile_header(monsoon::xdr::xdr_ostream& out,
    std::tuple<time_point, time_point> range) {
  encode_timestamp(out, std::get<0>(range));
  encode_timestamp(out, std::get<1>(range));
}

template<typename T>
std::uint32_t dictionary<T>::encode(const T& v) {
  const auto map_iter = encode_map_.find(v);
  if (map_iter != encode_map_.end()) return map_iter->second;

  if (decode_map_.size() > std::numeric_limits<std::uint32_t>::max())
    throw xdr::xdr_exception("too many dictionary entries");
  const std::uint32_t new_idx = decode_map_.size();

  decode_map_.push_back(v);
  try {
    encode_map_.emplace(v, new_idx);
  } catch (...) {
    decode_map_.pop_back();
    throw;
  }
  return new_idx;
}

template<typename T>
const T& dictionary<T>::decode(std::uint32_t idx) const {
  try {
    return decode_map_.at(idx);
  } catch (const std::out_of_range&) {
    throw xdr::xdr_exception("dictionary lookup failed");
  }
}

template<typename T>
bool dictionary<T>::update_pending() const noexcept {
  return update_start_ < decode_map_.size();
}


std::vector<std::string> decode_path(monsoon::xdr::xdr_istream& in) {
  return in.template get_collection<std::vector<std::string>>(
      [](monsoon::xdr::xdr_istream& in) {
        return in.get_string();
      });
}

void encode_path(monsoon::xdr::xdr_ostream& out,
    const metric_name& p) {
  out.put_collection(
      [](monsoon::xdr::xdr_ostream& out, std::string_view elem) {
        out.put_string(elem);
      },
      p.begin(), p.end());
}

void encode_path(monsoon::xdr::xdr_ostream& out,
    const simple_group& p) {
  out.put_collection(
      [](monsoon::xdr::xdr_ostream& out, std::string_view elem) {
        out.put_string(elem);
      },
      p.begin(), p.end());
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
  std::visit(
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
          [&out, &dict](const std::string_view& v) {
            out.put_uint32(static_cast<std::uint32_t>(metrickind::STRING));
            out.put_uint32(dict.encode(std::string(v.begin(), v.end())));
          },
          [&out](const histogram& v) {
            out.put_uint32(static_cast<std::uint32_t>(metrickind::HISTOGRAM));
            encode_histogram(out, v);
          }),
      value.get());
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

tags decode_tags(monsoon::xdr::xdr_istream& in,
    const dictionary<std::string>& dict) {
  using collection_type = std::vector<std::pair<std::string, metric_value>>;

  return tags(in.template get_collection<tags::map_type>(
      [&dict](monsoon::xdr::xdr_istream& in) {
        auto key = in.get_string();
        auto value = decode_metric_value(in, dict);
        return std::make_pair(std::move(key), std::move(value));
      }));
}

void encode_tags(monsoon::xdr::xdr_ostream& out, const tags& t,
    dictionary<std::string>& dict) {
  out.put_collection(
      [&dict](monsoon::xdr::xdr_ostream& out, auto tagentry) {
        out.put_string(tagentry.first);
        encode_metric_value(out, tagentry.second, dict);
      },
      t.begin(), t.end());
}

time_series_value decode_time_series_value(xdr::xdr_istream& in,
    const dictionary_delta& dict) {
  const std::uint32_t group_ref = in.get_uint32();
  const std::uint32_t tag_ref = in.get_uint32();
  return time_series_value(
      group_name(dict.gdd.decode(group_ref), dict.tdd.decode(tag_ref)),
      in.template get_collection<time_series_value::metric_map>(
          [&dict](xdr::xdr_istream& in) {
            const std::uint32_t metric_ref = in.get_uint32();
            return std::make_pair(
                dict.mdd.decode(metric_ref),
                decode_metric_value(in, dict.sdd));
          }));
}

void encode_time_series_value(xdr::xdr_ostream& out,
    const time_series_value& tsv,
    dictionary_delta& dict) {
  out.put_uint32(dict.gdd.encode(tsv.get_name().get_path()));
  out.put_uint32(dict.tdd.encode(tsv.get_name().get_tags()));
  const auto& metrics = tsv.get_metrics();
  out.put_collection(
      [&dict](xdr::xdr_ostream& out, const auto& entry) {
        out.put_uint32(dict.mdd.encode(entry.first));
        encode_metric_value(out, entry.second, dict.sdd);
      },
      metrics.begin(), metrics.end());
}

time_point decode_timestamp(monsoon::xdr::xdr_istream& in) {
  return time_point(in.get_int64());
}

void encode_timestamp(monsoon::xdr::xdr_ostream& out, const time_point& tp) {
  out.put_int64(tp.millis_since_posix_epoch());
}

time_series decode_time_series(xdr::xdr_istream& in,
    dictionary_delta& dict) {
  auto ts = decode_timestamp(in);
  if (in.get_bool()) dict.decode_update(in);
  return time_series(
      std::move(ts),
      in.template get_collection<time_series::tsv_set>(
          [&dict](xdr::xdr_istream& in) {
            return decode_time_series_value(in, dict);
          }));
}

void encode_time_series(xdr::xdr_ostream& out,
    const time_series& tsdata,
    dictionary_delta& dict) {
  encode_timestamp(out, tsdata.get_time());

  // Encode time_series_values into side buffer,
  // since we need to write dictionary ahead of time,
  // but the act of writing may cause updates
  // to the dictionary that have to be serialized first.
  auto pre_encoded_tsvs = xdr::xdr_bytevector_ostream<>();
  const auto& tsvs = tsdata.get_data();
  pre_encoded_tsvs.put_collection(
      [&dict](xdr::xdr_ostream& out, const auto& tsv) {
        encode_time_series_value(out, tsv, dict);
      },
      tsvs.begin(), tsvs.end());

  out.put_bool(dict.update_pending());
  if (dict.update_pending()) dict.encode_update(out);
  pre_encoded_tsvs.copy_to(out);
}


}}} /* namespace monsoon::history::v1 */
