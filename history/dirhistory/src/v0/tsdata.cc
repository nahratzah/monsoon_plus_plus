#include "tsdata.h"
#include <monsoon/gzip_stream.h>
#include <monsoon/positional_stream.h>

namespace monsoon {
namespace history {
namespace v0 {


tsdata_v0::tsdata_v0(fd&& file)
: file_(std::move(file)),
  gzipped_(is_gzip_file(file_))
{
  if (gzipped_) {
    auto r = gzip_decompress_reader<positional_reader>(positional_reader(file_));
  } else {
    auto r = positional_reader(file_);
  }

  ... // Set up xdr stream reader, gzipped iff gzipped_ is true, read mimeheader and timestamps
}

tsdata_v0::~tsdata_v0() noexcept {}


enum class metrickind : std::uint32_t {
  BOOL = 0,
  INT = 1,
  FLOAT = 2,
  STRING = 3,
  HISTOGRAM = 4,
  EMPTY = 0x7fffffff
};

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

std::vector<std::string> decode_path(monsoon::xdr::xdr_istream& in) {
  return in.template get_collection<std::vector<std::string>>(
      [](monsoon::xdr::xdr_istream& in) {
        return in.get_string();
      });
}

void encode_path(monsoon::xdr::xdr_ostream& out,
    const std::vector<std::string>& p) {
  out.put_collection(
      [](monsoon::xdr::xdr_ostream& out, const std::string& elem) {
        out.put_string(elem);
      },
      p.begin(), p.end());
}

metric_value decode_metric_value(monsoon::xdr::xdr_istream& in) {
  switch (in.get_uint32()) {
    default:
      throw monsoon::xdr::xdr_exception();
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
      return metric_value(in.get_string());
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
    const metric_value& value) {
  const auto& opt_raw = value.get();
  if (!opt_raw) {
    out.put_uint32(static_cast<std::uint32_t>(metrickind::EMPTY));
    return;
  }

  visit(opt_raw.get(),
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
      [&out](const std::string& v) {
        out.put_uint32(static_cast<std::uint32_t>(metrickind::STRING));
        out.put_string(v);
      },
      [&out](const histogram& v) {
        out.put_uint32(static_cast<std::uint32_t>(metrickind::HISTOGRAM));
        encode_histogram(out, v);
      });
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

time_series decode_time_series(monsoon::xdr::xdr_istream& in) {
  time_point timestamp = decode_timestamp(in);
  std::vector<time_series_value> tsvalues;

  in.template accept_collection(
      [](monsoon::xdr::xdr_istream& in) {
        const auto path = decode_path(in);
        return in.template get_collection<std::vector<time_series_value>>(
            [&path](monsoon::xdr::xdr_istream& in) {
              auto group_tags = decode_tags(in);
              auto metric_map = decode_metric_map(in);
              return time_series_value(
                  group_name(
                      simple_group(path.begin(), path.end()),
                      std::move(group_tags)),
                  std::make_move_iterator(metric_map.begin()),
                  std::make_move_iterator(metric_map.end()));
            });
      },
      [&tsvalues](auto&& tsv) {
        std::copy(
            std::make_move_iterator(tsv.begin()),
            std::make_move_iterator(tsv.end()),
            std::back_inserter(tsvalues));
      });

  return time_series(std::move(timestamp),
      std::make_move_iterator(tsvalues.begin()),
      std::make_move_iterator(tsvalues.end()));
}

void encode_time_series(monsoon::xdr::xdr_ostream& out,
    const time_series& ts) {
  using metrics_map = std::vector<std::pair<metric_name, metric_value>>;
  using tag_map = std::unordered_map<tags, metrics_map>;
  using group_map = std::unordered_map<simple_group, tag_map>;

  group_map data;
  for (const time_series_value& tsv : ts.get_data()) {
    const auto& group_name = tsv.get_name();
    const auto& metrics = tsv.get_metrics();

    std::copy(metrics.begin(), metrics.end(),
        std::back_inserter(
            data[group_name.get_path()][group_name.get_tags()]));
  }

  out.put_int64(ts.get_time().millis_since_posix_epoch());
  out.put_collection(
      [](monsoon::xdr::xdr_ostream& out, auto& group_entry) {
        encode_path(out, group_entry.first.get_path());
        out.put_collection(
            [](monsoon::xdr::xdr_ostream& out, auto& tag_entry) {
              encode_tags(out, tag_entry.first);
              out.put_collection(
                  [](monsoon::xdr::xdr_ostream& out, auto& metrics_entry) {
                    encode_path(out, metrics_entry.first.get_path());
                    encode_metric_value(out, metrics_entry.second);
                  },
                  tag_entry.second.begin(), tag_entry.second.end());
            },
            group_entry.second.begin(), group_entry.second.end());
      },
      data.begin(), data.end());
}

std::vector<metric_map_entry> decode_metric_map(
    monsoon::xdr::xdr_istream& in) {
  return in.get_collection<std::vector<metric_map_entry>>(
      [](monsoon::xdr::xdr_istream& in) {
        auto key = decode_path(in);
        auto value = decode_metric_value(in);
        return metric_map_entry(
            metric_name(
                std::make_move_iterator(key.begin()),
                std::make_move_iterator(key.end())),
            std::move(value));
      });
}

time_point decode_timestamp(monsoon::xdr::xdr_istream& in) {
  return time_point(in.get_int64());
}

void encode_timestamp(monsoon::xdr::xdr_ostream& out, const time_point& tp) {
  out.put_int64(tp.millis_since_posix_epoch());
}

tags decode_tags(monsoon::xdr::xdr_istream& in) {
  using collection_type = std::vector<std::pair<std::string, metric_value>>;

  auto tag_entries = in.template get_collection<collection_type>(
      [](monsoon::xdr::xdr_istream& in) {
        auto key = in.get_string();
        auto value = decode_metric_value(in);
        return std::make_pair(std::move(key), std::move(value));
      });
  return tags(
      std::make_move_iterator(tag_entries.begin()),
      std::make_move_iterator(tag_entries.end()));
}

void encode_tags(monsoon::xdr::xdr_ostream& out, const tags& t) {
  out.put_collection(
      [](monsoon::xdr::xdr_ostream& out, auto tagentry) {
        out.put_string(tagentry.first);
        encode_metric_value(out, tagentry.second);
      },
      t.begin(), t.end());
}


}}} /* namespace monsoon::history::v0 */
