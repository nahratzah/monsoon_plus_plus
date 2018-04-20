#include <monsoon/collector.h>

namespace monsoon {


collector::~collector() noexcept {}


sync_collector::~sync_collector() noexcept {}

auto sync_collector::provides() const
-> names_set {
  return { names_, {} };
}

auto sync_collector::run(objpipe::reader<time_point> tp_pipe) const
-> objpipe::reader<collection> {
  auto self = this->shared_from_this();
  return std::move(tp_pipe)
      .transform(
          [self](time_point tp) -> collection {
            return { tp, self->do_collect(), true };
          });
}


} /* namespace monsoon */
