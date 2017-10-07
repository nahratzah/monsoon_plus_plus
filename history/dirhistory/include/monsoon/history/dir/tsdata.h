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

  /**
   * Return the file version.
   */
  virtual auto version() const noexcept
      -> std::tuple<std::uint16_t, std::uint16_t> = 0;
  /**
   * Read all time series.
   *
   * Note: this is inefficient and potentially requires a lot of memory.
   * It is mainly intended for testing/debugging.
   */
  virtual std::vector<time_series> read_all() const = 0;

  /**
   * Test if the tsdata can handle appending a single time_series at a time.
   */
  virtual bool is_writable() const noexcept = 0;

  /**
   * Returns the range of timestamps (inclusive) this tsdata covers.
   */
  virtual auto time() const -> std::tuple<time_point, time_point> = 0;
};


}} /* namespace monsoon::history */

#endif /* MONSOON_HISTORY_DIR_TSDATA_H */
