#ifndef MONSOON_RULE_H
#define MONSOON_RULE_H

#include <monsoon/metric_source.h>

namespace monsoon {


class rule {
 public:
  virtual ~rule() noexcept;

  virtual void operator()(const metric_source&) const = 0;
};


} /* namespace monsoon */

#endif /* MONSOON_RULE_H */
