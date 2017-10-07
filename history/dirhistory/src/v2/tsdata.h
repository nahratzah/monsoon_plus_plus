#ifndef V2_TSDATA_H
#define V2_TSDATA_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <monsoon/history/dir/tsdata.h>
#include <monsoon/io/fd.h>
#include <monsoon/time_point.h>
#include <memory>
#include "file_segment_ptr.h"

namespace monsoon {
namespace history {
namespace v2 {


class monsoon_dirhistory_local_ tsdata_v2
: public tsdata
{
 public:
  struct carg;

  static constexpr std::uint16_t MAJOR = 2u;
  static constexpr std::uint16_t MAX_MINOR = 0u;

  static std::shared_ptr<tsdata_v2> open(io::fd&&);
  static std::shared_ptr<tsdata_v2> new_list_file(io::fd&&, time_point tp);

  tsdata_v2(const carg&);
  ~tsdata_v2() noexcept override;

  virtual std::shared_ptr<io::fd> fd() const noexcept = 0;
  std::vector<time_series> read_all() const override final;
  std::tuple<std::uint16_t, std::uint16_t> version() const noexcept override;
  auto time() const -> std::tuple<time_point, time_point> override;

 protected:
  inline auto hdr_file_size() const noexcept { return file_size_; }
  void update_hdr(time_point, time_point, const file_segment_ptr&,
      io::fd::size_type);

 private:
  virtual std::vector<time_series> read_all_raw_() const = 0;

  time_point first_, last_;
  std::uint32_t flags_, reserved_;
  std::uint64_t file_size_;
  std::uint16_t minor_version_;
};


}}} /* namespace monsoon::history::v2 */

#endif /* V2_TSDATA_H */
