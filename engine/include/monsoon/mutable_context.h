#ifndef MONSOON_MUTABLE_CONTEXT_H
#define MONSOON_MUTABLE_CONTEXT_H

#include <monsoon/context.h>
#include <monsoon/time_series.h>

namespace monsoon {


class mutable_context
: public context
{
 public:
  mutable_context() noexcept = default;
  ~mutable_context() noexcept override;

  virtual time_series& current() noexcept = 0;

 protected:
  mutable_context(const mutable_context&) noexcept = default;
  mutable_context& operator=(const mutable_context&) noexcept = default;
};


} /* namespace monsoon */

#endif /* MONSOON_MUTABLE_CONTEXT_H */
