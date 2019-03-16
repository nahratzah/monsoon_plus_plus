#include <monsoon/history/dir/tsdata.h>
#include <monsoon/io/gzip_stream.h>
#include <monsoon/io/positional_stream.h>
#include <monsoon/xdr/xdr_stream.h>
#include <objpipe/of.h>
#include <optional>
#include "tsdata_mime.h"
#include "v0/tsdata.h"
#include "v1/tsdata.h"
#include "v2/tsdata.h"

namespace monsoon {
namespace history {
namespace {


std::optional<tsfile_mimeheader> get_mimeheader_from_gzip(const io::fd& fd) {
  using pos = io::positional_reader;
  using gzip = io::gzip_decompress_reader<pos>;
  using xdr = xdr::xdr_stream_reader<gzip>;

  auto r = xdr(gzip(pos(fd)));
  return tsfile_mimeheader::read(r);
}

std::optional<tsfile_mimeheader> get_mimeheader_from_plain(const io::fd& fd) {
  using pos = io::positional_reader;
  using xdr = xdr::xdr_stream_reader<pos>;

  auto r = xdr(pos(fd));
  return tsfile_mimeheader::read(r);
}


} /* namespace monsoon::history::<anonymous> */


tsdata::~tsdata() noexcept {}

auto tsdata::open(const std::string& fname, io::fd::open_mode mode)
-> std::shared_ptr<tsdata> {
  return open(io::fd(fname, mode));
}

auto tsdata::open(io::fd&& fd) -> std::shared_ptr<tsdata> {
  bool is_gzipped = io::is_gzip_file(io::positional_reader(fd));

  const auto opt_hdr = (is_gzipped
      ? get_mimeheader_from_gzip(fd)
      : get_mimeheader_from_plain(fd));
  if (!opt_hdr.has_value()) return nullptr;
  const auto& hdr = opt_hdr.value();

  switch (hdr.major_version) {
    case v0::tsdata_v0::MAJOR:
      return std::make_shared<v0::tsdata_v0>(std::move(fd));
    case v1::tsdata_v1::MAJOR:
      return std::make_shared<v1::tsdata_v1>(std::move(fd));
    case v2::tsdata_v2::MAJOR:
      return v2::tsdata_v2::open(std::move(fd));
  }
  return nullptr; // XXX should not reach this
}

bool tsdata::is_tsdata(const std::string& fname) {
  return is_tsdata(io::fd(fname, io::fd::READ_ONLY));
}

bool tsdata::is_tsdata(const io::fd& fd) {
  bool is_gzipped = io::is_gzip_file(io::positional_reader(fd));

  const auto opt_hdr = (is_gzipped
      ? get_mimeheader_from_gzip(fd)
      : get_mimeheader_from_plain(fd));
  return opt_hdr.has_value();
}

auto tsdata::new_file(io::fd&& fd, std::uint16_t version)
-> std::shared_ptr<tsdata> {
  switch (version) {
    default:
      throw std::invalid_argument("version");
    case v0::tsdata_v0::MAJOR:
      return v0::tsdata_v0::new_file(std::move(fd), time_point::now());
    case v1::tsdata_v1::MAJOR:
      return v1::tsdata_v1::new_file(std::move(fd), time_point::now());
    case v2::tsdata_v2::MAJOR:
      return v2::tsdata_v2::new_list_file(std::move(fd), time_point::now());
  }
}

auto tsdata::new_file(io::fd&& fd) -> std::shared_ptr<tsdata> {
  return new_file(std::move(fd), v2::tsdata_v2::MAJOR);
}

auto tsdata::make_time_series(const metric_source::metric_emit& c) -> time_series {
  std::unordered_map<group_name, time_series_value> tsv_map;
  const time_point tp = std::get<0>(c);
  std::for_each(
      std::get<1>(c).begin(),
      std::get<1>(c).end(),
      [&tsv_map](const auto& e) {
        const auto& group_name = std::get<0>(e.first);
        const auto& metric_name = std::get<1>(e.first);
        const auto& metric_value = e.second;

#if __cplusplus >= 201703
        time_series_value& tsv =
            tsv_map.try_emplace(group_name, group_name).first->second;
#else
        time_series_value& tsv = tsv_map.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(group_name),
            std::forward_as_tuple(group_name)
            ).first->second;
#endif
        tsv.metrics()[metric_name] = metric_value;
      });
  auto ts_elems = objpipe::of(std::move(tsv_map))
      .iterate()
      .select_second();

  return time_series(tp, ts_elems.begin(), ts_elems.end());
}


}} /* namespace monsoon::history */
