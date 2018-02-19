#ifndef MONSOON_OBJPIPE_DETAIL_OF_PIPE_H
#define MONSOON_OBJPIPE_DETAIL_OF_PIPE_H

///\file
///\ingroup objpipe_detail

#include <optional>
#include <type_traits>
#include <utility>
#include <monsoon/objpipe/errc.h>
#include <monsoon/objpipe/detail/transport.h>

namespace monsoon::objpipe::detail {


/**
 * \brief Object pipe containing a single element.
 * \ingroup objpipe_detail
 *
 * \details The object pipe yields the single element it was constructed with.
 * \sa \ref monsoon::objpipe::of
 */
template<typename T>
class of_pipe {
 public:
  explicit constexpr of_pipe(T&& v)
  noexcept(std::is_nothrow_move_constructible_v<T>)
  : val_(std::in_place, std::move(v))
  {}

  explicit constexpr of_pipe(const T& v)
  noexcept(std::is_nothrow_copy_constructible_v<T>)
  : val_(std::in_place, v)
  {}

  constexpr auto is_pullable() const
  noexcept
  -> bool {
    return val_.has_value();
  }

  constexpr auto wait() const
  noexcept
  -> objpipe_errc {
    return (val_.has_value() ? objpipe_errc::success : objpipe_errc::closed);
  }

  ///\note An rvalue reference is returned, since front() is only allowed to be called at most once before pop_front() or pull().
  constexpr auto front() const
  noexcept
  -> transport<std::add_rvalue_reference_t<T>> {
    if (val_.has_value())
      return { std::in_place_index<0>, *val_ };
    return { std::in_place_index<1>, objpipe_errc::closed };
  }

  constexpr auto pop_front()
  noexcept
  -> objpipe_errc {
    if (val_.has_value()) {
      val_.reset();
      return objpipe_errc::success;
    } else {
      return objpipe_errc::closed;
    }
  }

  constexpr auto try_pull()
  noexcept
  -> transport<T> {
    return pull();
  }

  constexpr auto pull()
  noexcept
  -> transport<T> {
    if (!val_.has_value())
      return { std::in_place_index<1>, objpipe_errc::closed };
    auto rv = transport<T>(std::in_place_index<0>, *std::move(val_));
    val_.reset();
    return rv;
  }

 private:
  std::optional<T> val_;
};

template<typename T>
class of_pipe<std::reference_wrapper<T>> {
 public:
  explicit constexpr of_pipe(std::reference_wrapper<T> ref)
  noexcept
  : val_(&ref.get())
  {}

  constexpr auto is_pullable() const
  noexcept
  -> bool {
    return val_ != nullptr;
  }

  constexpr auto wait() const
  noexcept
  -> objpipe_errc {
    return (val_ != nullptr ? objpipe_errc::success : objpipe_errc::closed);
  }

  constexpr auto front() const
  noexcept
  -> transport<std::add_lvalue_reference_t<T>> {
    if (val_ != nullptr)
      return { std::in_place_index<0>, *val_ };
    return { std::in_place_index<1>, objpipe_errc::closed };
  }

  constexpr auto pop_front()
  noexcept
  -> objpipe_errc {
    return (std::exchange(val_, nullptr) != nullptr
        ? objpipe_errc::success
        : objpipe_errc::closed);
  }

  constexpr auto try_pull()
  noexcept
  -> transport<std::add_lvalue_reference_t<T>> {
    return pull();
  }

  constexpr auto pull()
  noexcept
  -> transport<std::add_lvalue_reference_t<T>> {
    if (val_ == nullptr)
      return { std::in_place_index<1>, objpipe_errc::closed };
    return { std::in_place_index<0>, *std::exchange(val_, nullptr) };
  }

 private:
  T* val_;
};


} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_OF_PIPE_H */
