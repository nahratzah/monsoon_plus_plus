#ifndef MONSOON_CONTEXT_H
#define MONSOON_CONTEXT_H

#include <monsoon/time_series.h>

namespace monsoon {


class context {
 public:
  context() noexcept = default;
  virtual ~context() noexcept;

  virtual const time_series& current() const noexcept = 0;

 protected:
  context(const context&) noexcept = default;
  context(context&&) noexcept {}
};


} /* namespace monsoon */

#endif /* MONSOON_CONTEXT_H */
