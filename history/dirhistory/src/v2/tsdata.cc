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
#include "../raw_file_segment_reader.h"
#include "../tsdata_mime.h"
#include "../overload.h"
#include "tsdata_tables.h"
#include "tsdata_list.h"

namespace monsoon {
namespace history {
namespace v2 {


struct monsoon_dirhistory_local_ tsdata_v2::carg {
  carg(const tsfile_mimeheader& mime, const tsfile_header& hdr)
  : mime(mime),
    hdr(hdr)
  {}

  const tsfile_mimeheader& mime;
  const tsfile_header& hdr;
};


std::shared_ptr<tsdata_v2> tsdata_v2::open(io::fd&& fd_) {
  auto fd = std::make_shared<io::fd>(std::move(fd_));

  try {
    xdr::xdr_stream_reader<raw_file_segment_reader> xdr = raw_file_segment_reader(*fd, 0, tsfile_mimeheader::XDR_ENCODED_LEN + tsfile_header::XDR_SIZE);
    auto mime = tsfile_mimeheader(xdr);
    auto hdr = tsfile_header(xdr, fd);
    const carg constructor_arg = carg(mime, hdr);
    xdr.close();

    return visit(
        overload(
            [&constructor_arg](file_segment<tsdata_list>&& data) -> std::shared_ptr<tsdata_v2> {
              return std::make_shared<tsdata_v2_list>(std::move(data), constructor_arg);
            },
            [&constructor_arg](file_segment<file_data_tables>&& data) -> std::shared_ptr<tsdata_v2> {
              return std::make_shared<tsdata_v2_tables>(std::move(data), constructor_arg);
            }),
        std::move(hdr).fdt());
  } catch (...) {
    fd_ = std::move(*fd);
    throw;
  }
}

tsdata_v2::tsdata_v2(const carg& arg)
: first_(arg.hdr.first()),
  last_(arg.hdr.last()),
  flags_(arg.hdr.flags()),
  file_size_(arg.hdr.file_size()),
  minor_version_(arg.mime.minor_version)
{}

tsdata_v2::~tsdata_v2() noexcept {}

std::vector<time_series> tsdata_v2::read_all() const {
  using namespace std::placeholders;

  std::vector<time_series> result = read_all_raw_();

  // only sort if flag sorted is false
  if ((flags_ & header_flags::SORTED) == 0u) {
    std::stable_sort(result.begin(), result.end(),
        std::bind(std::less<time_point>(),
            std::bind(&time_series::get_time, _1),
            std::bind(&time_series::get_time, _2)));
  }

  // only merge if flag distinct is false
  if ((flags_ & header_flags::DISTINCT) == 0u) {
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
  return std::make_tuple(MAJOR_VERSION, minor_version_);
}


}}} /* namespace monsoon::history::v2 */
