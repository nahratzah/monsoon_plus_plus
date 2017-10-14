#ifndef MONSOON_HISTORY_DIR_TSDATA_H
#define MONSOON_HISTORY_DIR_TSDATA_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <monsoon/io/fd.h>
#include <memory>
#include <string>
#include <vector>
#include <tuple>
#include <monsoon/time_series.h>
#include <monsoon/history/collect_history.h>

namespace monsoon {
namespace history {


class monsoon_dirhistory_export_ tsdata {
 public:
  using metrics_hash = collect_history::metrics_hash;
  template<typename...> class emit_acceptor;

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

  virtual void push_back(const time_series&) = 0;

  virtual std::unordered_set<simple_group> simple_groups() const = 0;
  virtual std::unordered_set<group_name> group_names() const = 0;
  virtual std::unordered_set<std::tuple<simple_group, metric_name>, metrics_hash>
      untagged_metrics() const = 0;
  virtual std::unordered_set<std::tuple<group_name, metric_name>, metrics_hash>
      tagged_metrics() const = 0;

  virtual void emit(
      emit_acceptor<group_name, metric_name, metric_value>&,
      std::optional<time_point>,
      std::optional<time_point>,
      const std::function<bool(const group_name&)>&,
      const std::function<bool(const group_name&, const metric_name&)>&)
      const = 0;
  virtual void emit(
      emit_acceptor<group_name, metric_name, metric_value>&,
      std::optional<time_point>,
      std::optional<time_point>,
      const std::function<bool(const simple_group&)>&,
      const std::function<bool(const simple_group&, const metric_name&)>&)
      const;
};

template<typename... Types>
class tsdata::emit_acceptor {
 public:
  using tuple_type = std::tuple<std::remove_cv_t<std::remove_reference_t<Types>>...>;
  using vector_type = std::vector<tuple_type>;

  virtual ~emit_acceptor() noexcept {}
  virtual void accept(time_point, vector_type) = 0;
};


}} /* namespace monsoon::history */

#endif /* MONSOON_HISTORY_DIR_TSDATA_H */
