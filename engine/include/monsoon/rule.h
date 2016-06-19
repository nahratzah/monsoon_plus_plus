#ifndef MONSOON_RULE_H
#define MONSOON_RULE_H

#include <monsoon/mutable_context.h>

namespace monsoon {


class rule {
 public:
  virtual ~rule() noexcept;

  virtual void operator()(mutable_context&) const = 0;
};


} /* namespace monsoon */

#endif /* MONSOON_RULE_H */
