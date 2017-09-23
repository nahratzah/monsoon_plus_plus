#ifndef MONSOON_HISTORY_DIR_TSDATA_H
#define MONSOON_HISTORY_DIR_TSDATA_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <monsoon/io/fd.h>
#include <memory>
#include <string>

namespace monsoon {
namespace history {


class monsoon_dirhistory_export_ tsdata {
 public:
  virtual ~tsdata() noexcept;

  monsoon_dirhistory_export_
  static auto open(const std::string&, io::fd::open_mode = io::fd::READ_ONLY)
      -> std::shared_ptr<tsdata>;
};


}} /* namespace monsoon::history */

#endif /* MONSOON_HISTORY_DIR_TSDATA_H */
