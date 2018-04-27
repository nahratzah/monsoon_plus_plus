#include "tsdata.h"
#include "encdec.h"
#include <functional>
#include <algorithm>
#include <monsoon/time_series.h>
#include <monsoon/time_series_value.h>
#include <monsoon/time_point.h>
#include <monsoon/group_name.h>
#include <monsoon/metric_name.h>
#include <monsoon/metric_value.h>
#include <monsoon/xdr/xdr_stream.h>
#include <monsoon/overload.h>
#include "../raw_file_segment_reader.h"
#include "../raw_file_segment_writer.h"
#include "../tsdata_mime.h"
#include "tsdata_tables.h"
#include "tsdata_list.h"

namespace monsoon {
namespace history {
namespace v2 {


std::shared_ptr<tsdata_v2> tsdata_v2::open(io::fd&& fd) {
  xdr::xdr_stream_reader<raw_file_segment_reader> xdr = raw_file_segment_reader(fd, 0, tsfile_mimeheader::XDR_ENCODED_LEN + tsfile_header::XDR_SIZE);
  tsfile_mimeheader mime(xdr);
  tsfile_header hdr;
  hdr.decode(xdr);
  xdr.close();

  switch (hdr.kind()) {
    default:
      throw xdr::xdr_exception("file kind not recognized");
    case header_flags::KIND_LIST:
      return std::make_shared<tsdata_v2_list>(std::move(fd), std::move(mime), std::move(hdr));
    case header_flags::KIND_TABLES:
      return std::make_shared<tsdata_v2_tables>(std::move(fd), std::move(mime), std::move(hdr));
  }
}

std::shared_ptr<tsdata_v2> tsdata_v2::new_list_file(io::fd&& fd,
    time_point tp) {
  constexpr auto HDR_LEN =
      tsfile_mimeheader::XDR_ENCODED_LEN + tsfile_header::XDR_SIZE;
  constexpr auto CHECKSUMMED_HDR_LEN =
      HDR_LEN + 4u;

  const std::uint32_t fl = (header_flags::KIND_LIST
      | header_flags::GZIP
      | header_flags::SORTED // Empty files are always sorted.
      | header_flags::DISTINCT); // Empty files are always distinct.

  io::fd::size_type data_len, storage_len;
  auto xdr = xdr::xdr_stream_writer<raw_file_segment_writer>(
      raw_file_segment_writer(fd, 0, &data_len, &storage_len));
  tsfile_mimeheader(MAJOR, MAX_MINOR).write(xdr);
  encode_timestamp(xdr, tp); // first
  encode_timestamp(xdr, tp); // last
  xdr.put_uint32(fl); // flags
  xdr.put_uint32(0u); // reserved
  xdr.put_uint64(CHECKSUMMED_HDR_LEN); // file size
  file_segment_ptr().encode(xdr); // nullptr == empty
  xdr.close();

  assert(data_len == HDR_LEN);
  assert(storage_len == CHECKSUMMED_HDR_LEN);

  return open(std::move(fd));
}

tsdata_v2::~tsdata_v2() noexcept {}

std::vector<time_series> tsdata_v2::read_all() const {
  using namespace std::placeholders;

  std::vector<time_series> result = read_all_raw_();

  // only sort if flag sorted is false
  if (!is_sorted()) {
    std::stable_sort(result.begin(), result.end(),
        std::bind(std::less<time_point>(),
            std::bind(&time_series::get_time, _1),
            std::bind(&time_series::get_time, _2)));
  }

  // only merge if flag distinct is false
  if (!is_distinct()) {
    // Merge successors with same timestamp together.
    auto result_back_iter = result.begin();
    auto result_end = result.end(); // New end of result.
    if (result_back_iter != result_end) {
      while (result_back_iter != result_end - 1) {
        if (result_back_iter[0].get_time() != result_back_iter[1].get_time()) {
          ++result_back_iter;
        } else {
          // Merge time_series.
          for (auto tsv_iter = std::make_move_iterator(result_back_iter[1].data().begin());
               tsv_iter != std::make_move_iterator(result_back_iter[1].data().end());
               ++tsv_iter) {
            bool tsv_inserted;
            time_series::tsv_set::iterator tsv_pos;
            std::tie(tsv_pos, tsv_inserted) = result_back_iter[0].data().insert(*tsv_iter);

            if (!tsv_inserted) { // Merge time_series_value into existing.
              auto tmp = *tsv_pos;
              result_back_iter[0].data().erase(tsv_pos);

              for (auto metric_iter = std::make_move_iterator(tsv_iter->get_metrics().begin());
                  metric_iter != std::make_move_iterator(tsv_iter->get_metrics().end());
                  ++metric_iter) {
                tmp.metrics()[metric_iter->first] = std::move(metric_iter->second);
              }

              bool replaced;
              std::tie(std::ignore, replaced) = result_back_iter[0].data().insert(std::move(tmp));
              assert(replaced);
            }
          }

          if (result_back_iter + 2 != result_end)
            std::rotate(result_back_iter + 1, result_back_iter + 2, result_end);
          --result_end;
        }
      }

      // Erase elements shifted backwards for deletion.
      result.erase(result_end, result.end());
    }
  }

  return result;
}

std::tuple<std::uint16_t, std::uint16_t> tsdata_v2::version() const noexcept {
  return std::make_tuple(mime_.major_version, mime_.minor_version);
}

auto tsdata_v2::time() const -> std::tuple<time_point, time_point> {
  return std::make_tuple(hdr_.first, hdr_.last);
}

auto tsdata_v2::get_path() const -> std::optional<std::string> {
  return fd_.get_path();
}

auto tsdata_v2::get_ctx() const -> encdec_ctx {
  return encdec_ctx(fd(), hdr_.flags);
}

void tsdata_v2::update_hdr(time_point lo, time_point hi,
    const file_segment_ptr& fsp, io::fd::size_type new_file_len) {
  constexpr auto HDR_LEN =
      tsfile_mimeheader::XDR_ENCODED_LEN + tsfile_header::XDR_SIZE;
  constexpr auto CHECKSUMMED_HDR_LEN =
      HDR_LEN + 4u;
  assert(lo <= hi);

  if (lo < hdr_.last)
    hdr_.flags &= ~header_flags::SORTED;
  if (lo <= hdr_.last)
    hdr_.flags &= ~header_flags::DISTINCT;

  mime_.major_version = MAJOR;
  mime_.minor_version = MAX_MINOR;
  if (lo < hdr_.first) hdr_.first = lo;
  if (hi > hdr_.last) hdr_.last = hi;
  hdr_.file_size = new_file_len;
  hdr_.fdt = fsp;

  io::fd::size_type data_len, storage_len;
  auto xdr = xdr::xdr_stream_writer<raw_file_segment_writer>(
      raw_file_segment_writer(fd_, 0, &data_len, &storage_len));
  mime_.write(xdr);
  hdr_.encode(xdr);
  xdr.close();
  fd_.flush();

  assert(data_len == HDR_LEN);
  assert(storage_len == CHECKSUMMED_HDR_LEN);
}

bool tsdata_v2::is_distinct() const noexcept {
  return (hdr_.flags & header_flags::DISTINCT) != 0u;
}

bool tsdata_v2::is_sorted() const noexcept {
  return (hdr_.flags & header_flags::SORTED) != 0u;
}


}}} /* namespace monsoon::history::v2 */
