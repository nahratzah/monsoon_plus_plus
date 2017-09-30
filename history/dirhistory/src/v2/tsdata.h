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
  static std::shared_ptr<tsdata_v2> open(io::fd&& fd_);

  ~tsdata_v2() noexcept override;

  virtual std::shared_ptr<io::fd> fd() const noexcept = 0;
  std::vector<time_series> read_all() const override final;

 private:
  virtual std::vector<time_series> read_all_raw_() const = 0;

  time_point first_, last_;
  std::uint32_t flags_, reserved_;
  std::uint64_t file_size_;
};


}}} /* namespace monsoon::history::v2 */

#endif /* V2_TSDATA_H */
