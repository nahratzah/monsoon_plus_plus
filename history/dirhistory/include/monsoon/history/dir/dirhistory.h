#ifndef MONSOON_HISTORY_DIR_DIRHISTORY_H
#define MONSOON_HISTORY_DIR_DIRHISTORY_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <monsoon/history/collect_history.h>
// XXX: create compiler cfg file...
// #include <filesystem>
#include <boost/filesystem.hpp>

namespace monsoon {
namespace dirhistory {

// XXX: create compiler cfg file...
// namespace filesystem = ::std::filesystem;
namespace filesystem = ::boost::filesystem;


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
