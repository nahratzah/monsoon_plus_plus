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
 private:
  using type = std::add_rvalue_reference_t<T>;
  using transport_type = transport<type>;

 public:
  explicit constexpr of_pipe(T&& v)
  noexcept(std::is_nothrow_move_constructible_v<T>)
  : val_(std::move(v))
  {}

  explicit constexpr of_pipe(const T& v)
  noexcept(std::is_nothrow_copy_constructible_v<T>)
  : val_(v)
  {}

  constexpr auto is_pullable() const
  noexcept
  -> bool {
    return !consumed_;
  }

  constexpr auto wait() const
  noexcept
  -> objpipe_errc {
    return (!consumed_ ? objpipe_errc::success : objpipe_errc::closed);
  }

  ///\note An rvalue reference is returned, since front() is only allowed to be called at most once before pop_front() or pull().
  constexpr auto front() const
  noexcept
  -> transport_type {
    if (consumed_)
      return transport_type(std::in_place_index<1>, objpipe_errc::closed);
    return transport_type(std::in_place_index<0>, std::move(val_));
  }

  constexpr auto pop_front()
  noexcept
  -> objpipe_errc {
    if (consumed_) return objpipe_errc::closed;
    consumed_ = true;
    return objpipe_errc::success;
  }

  constexpr auto try_pull()
  noexcept
  -> transport_type {
    return pull();
  }

  constexpr auto pull()
  noexcept
  -> transport_type {
    if (consumed_)
      return transport_type(std::in_place_index<1>, objpipe_errc::closed);
    consumed_ = true;
    return transport_type(std::in_place_index<0>, std::move(val_));
  }

 private:
  mutable bool consumed_ = false;
  mutable T val_;
};

template<typename T>
class of_pipe<std::reference_wrapper<T>> {
 private:
  using type = std::add_lvalue_reference_t<std::add_const_t<T>>;
  using transport_type = transport<type>;

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
  -> transport_type {
    if (val_ == nullptr)
      return transport_type(std::in_place_index<1>, objpipe_errc::closed);
    return transport_type(std::in_place_index<0>, *val_);
  }

  constexpr auto pop_front()
  noexcept
  -> objpipe_errc {
    if (val_ == nullptr) return objpipe_errc::closed;
    val_ = nullptr;
    return objpipe_errc::success;
  }

  constexpr auto try_pull()
  noexcept
  -> transport_type {
    return pull();
  }

  constexpr auto pull()
  noexcept
  -> transport_type {
    auto rv = front();
    val_ = nullptr;
    return rv;
  }

 private:
  std::add_const_t<T>* val_;
};


} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_OF_PIPE_H */
