#ifndef MONSOON_HISTORY_DYNAMICS_H
#define MONSOON_HISTORY_DYNAMICS_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <memory>
#include <utility>
#include <cassert>

namespace monsoon::history {


class monsoon_dirhistory_local_ dynamics {
 public:
  virtual ~dynamics() noexcept;
};

template<typename T>
class monsoon_dirhistory_local_ typed_dynamics
: dynamics
{
 public:
  explicit typed_dynamics(std::shared_ptr<T> parent)
  : dynamics(),
    parent_(std::move(parent))
  {
    assert(parent_ != nullptr);
  }

 protected:
  auto parent() -> T& {
    assert(parent_ != nullptr);
    return *parent_;
  }

  auto parent() const -> const T& {
    assert(parent_ != nullptr);
    return *parent_;
  }

 private:
  std::shared_ptr<T> parent_;
};

template<>
class monsoon_dirhistory_local_ typed_dynamics<void>
: dynamics
{
 public:
  [[deprecated]]
  typed_dynamics()
  : typed_dynamics(nullptr)
  {}

  explicit typed_dynamics(std::shared_ptr<const void> parent)
  : dynamics(),
    parent_(std::move(parent))
  {}

 private:
  std::shared_ptr<const void> parent_;
};


} /* namespace monsoon::history */

#endif /* MONSOON_HISTORY_DYNAMICS_H */
