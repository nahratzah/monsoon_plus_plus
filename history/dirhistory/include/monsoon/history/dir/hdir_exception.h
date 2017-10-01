#ifndef MONSOON_HISTORY_DIR_HDIR_EXCEPTION_H
#define MONSOON_HISTORY_DIR_HDIR_EXCEPTION_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <stdexcept>

namespace monsoon {
namespace history {


class monsoon_dirhistory_export_ dirhistory_exception
: public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
  ~dirhistory_exception() override;
};


}} /* namespace monsoon::history */

#endif /* MONSOON_HISTORY_DIR_EXCEPTION_H */
