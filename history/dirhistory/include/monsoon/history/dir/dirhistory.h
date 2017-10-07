#ifndef MONSOON_HISTORY_DIR_DIRHISTORY_H
#define MONSOON_HISTORY_DIR_DIRHISTORY_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <monsoon/history/collect_history.h>

#if __has_include(<filesystem>)
# include <filesystem>
#else
# include <boost/filesystem.hpp>
#endif

namespace monsoon {
namespace dirhistory {

#if __has_include(<filesystem>)
namespace filesystem = ::std::filesystem;
#else
namespace filesystem = ::boost::filesystem;
#endif


class monsoon_dirhistory_export_ dirhistory
: public collect_history
{
 private:
  const filesystem::path dir_;

 public:
  dirhistory(filesystem::path);
  ~dirhistory() noexcept override;

 private:
  dirhistory(const dirhistory&&) = delete;
  dirhistory(dirhistory&&) = delete;
};


}} /* namespace monsoon::dirhistory */

#endif /* MONSOON_HISTORY_DIR_DIRHISTORY_H */
