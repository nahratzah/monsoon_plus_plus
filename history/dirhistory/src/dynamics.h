#ifndef MONSOON_HISTORY_DYNAMICS_H
#define MONSOON_HISTORY_DYNAMICS_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <memory>
#include <utility>

namespace monsoon::history {


class monsoon_dirhistory_local_ dynamics {
 public:
  [[deprecated]]
  dynamics()
  : dynamics(nullptr)
  {}

  dynamics(std::shared_ptr<void> parent)
  : parent_(std::move(parent))
  {}

  virtual ~dynamics() noexcept;

 private:
  [[maybe_unused]]
  std::shared_ptr<void> parent_;
};


} /* namespace monsoon::history */

#endif /* MONSOON_HISTORY_DYNAMICS_H */
