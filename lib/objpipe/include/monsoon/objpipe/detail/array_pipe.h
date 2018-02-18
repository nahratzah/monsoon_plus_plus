#ifndef MONSOON_OBJPIPE_DETAIL_ARRAY_PIPE_H
#define MONSOON_OBJPIPE_DETAIL_ARRAY_PIPE_H

///\file
///\ingroup objpipe_detail

#include <deque>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <utility>
#include <variant>
#include <monsoon/objpipe/errc.h>

namespace monsoon::objpipe::detail {


/*
 * \brief Iterate over a copied sequence of elements.
 * \ingroup objpipe_detail
 *
 * \details
 * Elements are copied into a std::deque and released during iteration.
 * \tparam T The type of elements to iterate over.
 * \tparam Alloc The allocator to use for storing the elements.
 * \sa \ref monsoon::objpipe::new_array
 */
template<typename T, typename Alloc>
class array_pipe {
 public:
  template<typename Iter>
  array_pipe(Iter b, Iter e, Alloc alloc)
  : data_(b, e, alloc)
  {}

  array_pipe(std::initializer_list<T> init, Alloc alloc)
  : data_(init, alloc)
  {}

  auto is_pullable() const
  noexcept
  -> objpipe_errc {
    return !data_.empty();
  }

  auto wait() const
  noexcept(noexcept(data_.empty()))
  -> objpipe_errc {
    return (data_.empty() ? objpipe_errc::closed : objpipe_errc::success);
  }

  auto front() const
  noexcept(noexcept(data_.empty()) && noexcept(data_.front()))
  -> std::variant<T&&, objpipe_errc> {
    if (data_.empty()) return { std::in_place_index<1>, objpipe_errc::closed };
    return { std::in_place_index<0>, std::move(data_.front()) };
  }

  auto pop_front()
  noexcept(noexcept(data_.pop_front()))
  -> objpipe_errc {
    if (data_.empty()) return objpipe_errc::closed;
    data_.pop_front();
    return objpipe_errc::success;
  }

  auto try_pull()
  noexcept(noexcept(pull()))
  -> decltype(pull()) {
    return pull();
  }

  auto pull()
  noexcept(noexcept(data_.empty())
      && noexcept(data_.front())
      && noexcept(data_.pop_front())
      && std::is_nothrow_move_constructible_v<T>)
  -> std::variant<T, objpipe_errc> {
    if (data_.empty()) return { std::in_place_index<1>, objpipe_errc::closed };
    auto rv = std::variant<T, objpipe_errc>(std::in_place_index<0>, std::move(data_.front()));
    data_.pop_front();
    return rv;
  }

 private:
  mutable std::deque<T, Alloc> data_;
};


} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_ARRAY_PIPE_H */
