#include <monsoon/collectors/self.h>
#include <monsoon/path_matcher.h>
#include <monsoon/tag_matcher.h>
#include <monsoon/instrumentation.h>
#include <objpipe/reader.h>

namespace monsoon::collectors {


self::self() noexcept
{}

self::~self() noexcept {}

auto self::provides() const
-> names_set {
  return {
    {}, // No known names.
    { // Wildcarded root_path::**
      { path_matcher().push_back_double_wildcard(),
        tag_matcher(),
        path_matcher().push_back_double_wildcard() }
    }
  };
}

auto self::run(objpipe::reader<time_point> tp_pipe) const
-> objpipe::reader<collection> {

  return std::move(tp_pipe)
      .transform(
          [](const time_point& tp) {
            auto c = monsoon_instrumentation_collect();
            c.tp = tp;
            return c;
          });
}


} /* namespace monsoon::collectors */
