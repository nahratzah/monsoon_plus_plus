#ifndef V2_TSDATA_H
#define V2_TSDATA_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <monsoon/history/dir/tsdata.h>
#include <monsoon/io/fd.h>
#include <monsoon/time_point.h>
#include <memory>
#include "file_segment_ptr.h"
#include "../tsdata_mime.h"
#include "../dynamics.h"
#include "tsfile_header.h"
#include "encdec_ctx.h"

namespace monsoon {
namespace history {
namespace v2 {


class monsoon_dirhistory_local_ tsdata_v2
: public tsdata,
  public dynamics
{
 public:
  static constexpr std::uint16_t MAJOR = 2u;
  static constexpr std::uint16_t MAX_MINOR = 0u;

  static std::shared_ptr<tsdata_v2> open(io::fd&& fd);
  static std::shared_ptr<tsdata_v2> new_list_file(io::fd&& fd, time_point tp);

  tsdata_v2(io::fd&& fd, const tsfile_mimeheader& mime, const tsfile_header& hdr)
  : fd_(std::move(fd)),
    mime_(mime),
    hdr_(hdr)
  {}

  ~tsdata_v2() noexcept override;

  io::fd& fd() const noexcept { return fd_; }
  const file_segment_ptr fdt() const noexcept { return hdr_.fdt; }
  std::vector<time_series> read_all() const override final;
  std::tuple<std::uint16_t, std::uint16_t> version() const noexcept override;
  auto time() const -> std::tuple<time_point, time_point> override;
  std::optional<std::string> get_path() const override;
  auto get_ctx() const -> encdec_ctx;

 protected:
  inline auto hdr_file_size() const noexcept { return hdr_.file_size; }
  void update_hdr(time_point, time_point, const file_segment_ptr&,
      io::fd::size_type);
  bool is_distinct() const noexcept;
  bool is_sorted() const noexcept;

 private:
  virtual std::vector<time_series> read_all_raw_() const = 0;

  mutable io::fd fd_;
  tsfile_mimeheader mime_;
  tsfile_header hdr_;
};


}}} /* namespace monsoon::history::v2 */

#endif /* V2_TSDATA_H */
