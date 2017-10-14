#ifndef MONSOON_HISTORY_DIR_DIRHISTORY_H
#define MONSOON_HISTORY_DIR_DIRHISTORY_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <monsoon/history/collect_history.h>
#include <vector>
#include <memory>

#if __has_include(<filesystem>)
# include <filesystem>
#else
# include <boost/filesystem.hpp>
#endif

namespace monsoon {
namespace history {

#if __has_include(<filesystem>)
namespace filesystem = ::std::filesystem;
#else
namespace filesystem = ::boost::filesystem;
#endif

class tsdata;


class monsoon_dirhistory_export_ dirhistory
: public collect_history
{
 public:
  dirhistory(filesystem::path, bool = true);
  ~dirhistory() noexcept override;

  void push_back(const time_series&) override;

  auto time() const -> std::tuple<time_point, time_point> override;
  auto simple_groups(const time_range&) const
      -> std::unordered_set<simple_group> override;
  auto group_names(const time_range&) const
      -> std::unordered_set<group_name> override;
  auto tagged_metrics(const time_range&) const
      -> std::unordered_set<std::tuple<group_name, metric_name>, metrics_hash> override;
  auto untagged_metrics(const time_range&) const
      -> std::unordered_set<std::tuple<simple_group, metric_name>, metrics_hash> override;

  void emit(
      acceptor<group_name, metric_name, metric_value>&,
      time_range,
      std::function<bool(const group_name&)>,
      std::function<bool(const group_name&, const metric_name&)>,
      time_point::duration = time_point::duration(0)) const override;
  void emit(
      acceptor<group_name, metric_name, metric_value>&,
      time_range,
      std::function<bool(const simple_group&)>,
      std::function<bool(const simple_group&, const metric_name&)>,
      time_point::duration = time_point::duration(0)) const override;

 private:
  dirhistory(const dirhistory&&) = delete;
  dirhistory(dirhistory&&) = delete;

  monsoon_dirhistory_local_
  void maybe_start_new_file_(time_point);
  monsoon_dirhistory_local_
  static auto decide_fname_(time_point) -> filesystem::path;

  const filesystem::path dir_;
  std::vector<std::shared_ptr<tsdata>> files_;
  std::shared_ptr<tsdata> write_file_; // May be null.
  const bool writable_;
};


}} /* namespace monsoon::history */

#endif /* MONSOON_HISTORY_DIR_DIRHISTORY_H */
