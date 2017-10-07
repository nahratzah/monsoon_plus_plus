#include <monsoon/history/dir/tsdata.h>
#include <monsoon/io/gzip_stream.h>
#include <monsoon/io/positional_stream.h>
#include <monsoon/xdr/xdr_stream.h>
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
    case 0u:
      return std::make_shared<v0::tsdata_v0>(std::move(fd));
    case 1u:
      return std::make_shared<v1::tsdata_v1>(std::move(fd));
    case 2u:
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
    case 0u:
      return v0::tsdata_v0::new_file(std::move(fd), time_point::now());
    case 1u:
      return v1::tsdata_v1::new_file(std::move(fd), time_point::now());
    case 2u:
      return v2::tsdata_v2::new_list_file(std::move(fd), time_point::now());
  }
}


}} /* namespace monsoon::history */
