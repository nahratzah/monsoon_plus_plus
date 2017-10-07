#ifndef V2_TSDATA_H
#define V2_TSDATA_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <monsoon/history/dir/tsdata.h>
#include <monsoon/io/fd.h>
#include <memory>

namespace monsoon {
namespace history {
namespace v2 {


class monsoon_dirhistory_local_ tsdata_v2
: public tsdata
{
 public:
  struct carg;

  static constexpr std::uint16_t MAJOR_VERSION = 2u;

  static std::shared_ptr<tsdata_v2> open(io::fd&&);

  tsdata_v2(const carg&);
  ~tsdata_v2() noexcept override;

  virtual std::shared_ptr<io::fd> fd() const noexcept = 0;
  std::vector<time_series> read_all() const override final;
  std::tuple<std::uint16_t, std::uint16_t> version() const noexcept override;
  auto time() const -> std::tuple<time_point, time_point> override;

 private:
  virtual std::vector<time_series> read_all_raw_() const = 0;

  time_point first_, last_;
  std::uint32_t flags_, reserved_;
  std::uint64_t file_size_;
  std::uint16_t minor_version_;
};


}}} /* namespace monsoon::history::v2 */

#endif /* V2_TSDATA_H */
