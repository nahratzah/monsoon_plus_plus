#ifndef MONSOON_COLLECTORS_SELF_H
#define MONSOON_COLLECTORS_SELF_H

///\file
///\ingroup collectors

#include <monsoon/collector.h>

namespace monsoon::collectors {


class self
: public collector
{
  public:
  self() noexcept;
  ~self() noexcept override;

  auto provides() const -> names_set override;
  auto run(objpipe::reader<time_point> tp_pipe) const -> objpipe::reader<collection> override;
};


} /* namespace monsoon::collectors */

#endif /* MONSOON_COLLECTORS_SELF_H */
