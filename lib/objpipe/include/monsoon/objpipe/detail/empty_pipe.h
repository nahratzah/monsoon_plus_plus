#ifndef MONSOON_OBJPIPE_DETAIL_EMPTY_PIPE_H
#define MONSOON_OBJPIPE_DETAIL_EMPTY_PIPE_H

#include <monsoon/objpipe/errc.h>
#include <monsoon/objpipe/detail/transport.h>

namespace monsoon::objpipe::detail {


template<typename T>
class empty_pipe {
 public:
  constexpr auto wait()
  noexcept
  -> objpipe_errc {
    return objpipe_errc::closed;
  }

  constexpr auto is_pullable()
  noexcept
  -> bool {
    return false;
  }

  constexpr auto front()
  noexcept
  -> transport<T> {
    return transport<T>(std::in_place_index<1>, objpipe_errc::closed);
  }

  constexpr auto pop_front()
  noexcept
  -> objpipe_errc {
    return objpipe_errc::closed;
  }

  constexpr auto pull()
  noexcept
  -> transport<T> {
    return transport<T>(std::in_place_index<1>, objpipe_errc::closed);
  }

  constexpr auto try_pull()
  noexcept
  -> transport<T> {
    return transport<T>(std::in_place_index<1>, objpipe_errc::closed);
  }
};


} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_EMPTY_PIPE_H */
