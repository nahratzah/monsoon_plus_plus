#include <monsoon/history/dir/tsdata.h>
#include <monsoon/io/gzip_stream.h>
#include <monsoon/io/positional_stream.h>
#include <monsoon/xdr/xdr_stream.h>
#include "tsdata_mime.h"
#include "v0/tsdata.h"
#include "v1/tsdata.h"
#include "v2/tsdata.h"

namespace monsoon {
namespace history {
namespace {


tsfile_mimeheader get_mimeheader_from_gzip(io::fd& fd) {
  using pos = io::positional_reader;
  using gzip = io::gzip_decompress_reader<pos>;
  using xdr = xdr::xdr_stream_reader<gzip>;

  auto r = xdr(gzip(pos(fd)));
  return tsfile_mimeheader(r);
}

tsfile_mimeheader get_mimeheader_from_plain(io::fd& fd) {
  using pos = io::positional_reader;
  using xdr = xdr::xdr_stream_reader<pos>;

  auto r = xdr(pos(fd));
  return tsfile_mimeheader(r);
}


} /* namespace monsoon::history::<anonymous> */


tsdata::~tsdata() noexcept {}

auto tsdata::open(const std::string& fname, io::fd::open_mode mode)
-> std::shared_ptr<tsdata> {
  auto fd = io::fd(fname, mode);
  bool is_gzipped = io::is_gzip_file(io::positional_reader(fd));

  const auto hdr = (is_gzipped
      ? get_mimeheader_from_gzip(fd)
      : get_mimeheader_from_plain(fd));

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


}} /* namespace monsoon::history */
