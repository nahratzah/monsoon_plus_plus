#ifndef MONSOON_PUSH_PROCESSOR_H
#define MONSOON_PUSH_PROCESSOR_H

#include <monsoon/time_series.h>
#include <monsoon/alert.h>
#include <unordered_set>

namespace monsoon {


class push_processor {
 public:
  push_processor() = default;
  virtual ~push_processor() noexcept;

  virtual void process(const time_series&,
                       const std::unordered_set<alert>&) = 0;
};


} /* namespace monsoon */

#endif /* MONSOON_PUSH_PROCESSOR_H */
