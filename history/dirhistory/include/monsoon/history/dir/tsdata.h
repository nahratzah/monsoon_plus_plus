#ifndef MONSOON_HISTORY_DIR_TSDATA_H
#define MONSOON_HISTORY_DIR_TSDATA_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <monsoon/io/fd.h>
#include <memory>
#include <string>
#include <vector>
#include <tuple>
#include <monsoon/time_series.h>
#include <monsoon/metric_source.h>

namespace monsoon {
namespace history {


class monsoon_dirhistory_export_ tsdata {
 public:
  using emit_type = metric_source::metric_emit;

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

  monsoon_dirhistory_export_
  static auto new_file(io::fd&&, std::uint16_t) -> std::shared_ptr<tsdata>;
  monsoon_dirhistory_export_
  static auto new_file(io::fd&&) -> std::shared_ptr<tsdata>;

  /**
   * \brief Return the file version.
   */
  virtual auto version() const noexcept
  -> std::tuple<std::uint16_t, std::uint16_t> = 0;
  /**
   * \brief Read all time series.
   *
   * \note This is inefficient and potentially requires a lot of memory.
   * It is mainly intended for testing/debugging.
   */
  virtual std::vector<time_series> read_all() const = 0;

  /**
   * \brief Test if the tsdata can handle appending a single time_series at a time.
   */
  virtual bool is_writable() const noexcept = 0;

  /**
   * \brief Returns the range of timestamps (inclusive) this tsdata covers.
   */
  virtual auto time() const -> std::tuple<time_point, time_point> = 0;

  virtual void push_back(const emit_type& c) = 0;

  ///\brief Returns the path to the underlying file.
  virtual std::optional<std::string> get_path() const = 0;

  /**
   * \brief Emit metrics matching the given constraints.
   *
   * \details
   * Metrics are read from the file and emitted if they match the given constraints.
   * Metrics are emitted in ascending order to time stamp.
   *
   * \param[in] begin,end Describes the range of time stamps to emit.
   * \param[in] group_filter Only groups matching the filter will be emitted.
   * \param[in] tag_filter Only groups with matching tags will be emitted.
   * \param[in] metric_filter Only metric names matching the filter will be emitted.
   * \returns An objpipe containing maps of metrics, by timestamp,
   * in ascending order of time.
   */
  virtual auto emit(
      std::optional<time_point> begin,
      std::optional<time_point> end,
      const path_matcher& group_filter,
      const tag_matcher& tag_filter,
      const path_matcher& metric_filter)
      const
  -> objpipe::reader<emit_type> = 0;

  /**
   * \brief Emit timestamps between the given constraint (inclusive).
   *
   * \param[in] begin The lowest time point that may be emitted.
   * \param[in] end The highest time point that may be emitted.
   * \returns An objpipe containing all timestamps in this tsdata,
   * between the range [\p begin, \p end], in ascending order.
   */
  virtual auto emit_time(
      std::optional<time_point> begin,
      std::optional<time_point> end) const
  -> objpipe::reader<time_point> = 0;

 protected:
  /**
   * \brief Helper function to convert metrics to a time series.
   *
   * \param[in] c The data to be converted to a time series.
   * \returns A time series created from \p c.
   */
  static auto make_time_series(const metric_source::metric_emit& c)
  -> time_series;
};


}} /* namespace monsoon::history */

#endif /* MONSOON_HISTORY_DIR_TSDATA_H */
