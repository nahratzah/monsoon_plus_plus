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
  dirhistory(const dirhistory&&) = delete;
  dirhistory(dirhistory&&) = delete;

  const filesystem::path dir_;
  std::vector<std::shared_ptr<tsdata>> files_;
};


}} /* namespace monsoon::history */

#endif /* MONSOON_HISTORY_DIR_DIRHISTORY_H */
