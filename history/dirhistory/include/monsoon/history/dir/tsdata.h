#ifndef MONSOON_HISTORY_DIR_TSDATA_H
#define MONSOON_HISTORY_DIR_TSDATA_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <monsoon/io/fd.h>
#include <memory>
#include <string>
#include <vector>
#include <tuple>
#include <monsoon/time_series.h>

namespace monsoon {
namespace history {


class monsoon_dirhistory_export_ tsdata {
 public:
  virtual ~tsdata() noexcept;

  monsoon_dirhistory_export_
  static auto open(const std::string&, io::fd::open_mode = io::fd::READ_ONLY)
      -> std::shared_ptr<tsdata>;
  monsoon_dirhistory_export_
  static auto open(io::fd&&) -> std::shared_ptr<tsdata>;

  monsoon_dirhistory_export_
  static bool is_tsdata(const std::string&);
  monsoon_dirhistory_export_
  static bool is_tsdata(const io::fd&);

  virtual auto version() const noexcept
      -> std::tuple<std::uint16_t, std::uint16_t> = 0;
  virtual std::vector<time_series> read_all() const = 0;
};


}} /* namespace monsoon::history */

#endif /* MONSOON_HISTORY_DIR_TSDATA_H */
