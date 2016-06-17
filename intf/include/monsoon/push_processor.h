#ifndef MONSOON_PUSH_PROCESSOR_H
#define MONSOON_PUSH_PROCESSOR_H

#include <monsoon/time_series.h>

namespace monsoon {


class push_processor {
 public:
  push_processor() = default;
  virtual ~push_processor() noexcept;

  virtual void process(const time_series&) = 0;
};


} /* namespace monsoon */

#endif /* MONSOON_PUSH_PROCESSOR_H */
