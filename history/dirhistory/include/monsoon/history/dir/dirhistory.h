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

 private:
  void do_push_back_(const metric_emit&) override;

 public:
  auto time() const -> std::tuple<time_point, time_point> override;

  auto emit(
      time_range,
      path_matcher,
      tag_matcher,
      path_matcher,
      time_point::duration = time_point::duration(0)) const
  -> objpipe::reader<emit_type> override;
  auto emit_time(
      time_range,
      time_point::duration = time_point::duration(0)) const
  -> objpipe::reader<time_point> override;

 private:
  dirhistory(const dirhistory&&) = delete;
  dirhistory(dirhistory&&) = delete;

  monsoon_dirhistory_local_
  void maybe_start_new_file_(time_point);
  monsoon_dirhistory_local_
  static auto decide_fname_(time_point) -> filesystem::path;

  const filesystem::path dir_;
  std::shared_ptr<std::vector<std::shared_ptr<tsdata>>> files_ = std::make_shared<std::vector<std::shared_ptr<tsdata>>>();
  std::shared_ptr<tsdata> write_file_; // May be null.
  const bool writable_;

  std::shared_ptr<void> file_count_;
};


}} /* namespace monsoon::history */

#endif /* MONSOON_HISTORY_DIR_DIRHISTORY_H */
