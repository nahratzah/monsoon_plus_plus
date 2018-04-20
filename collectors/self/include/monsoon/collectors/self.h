#ifndef MONSOON_COLLECTORS_SELF_H
#define MONSOON_COLLECTORS_SELF_H

///\file
///\ingroup collectors

#include <monsoon/collector.h>
#include <instrumentation/group.h>

namespace monsoon::collectors {


class self
: public collector
{
 public:
  self() noexcept;
  self(const instrumentation::group& grp) noexcept;
  ~self() noexcept override;

  auto provides() const -> names_set override;
  auto run(objpipe::reader<time_point> tp_pipe) -> objpipe::reader<collection> override;

 private:
  const instrumentation::group& grp_;
};


} /* namespace monsoon::collectors */

#endif /* MONSOON_COLLECTORS_SELF_H */
