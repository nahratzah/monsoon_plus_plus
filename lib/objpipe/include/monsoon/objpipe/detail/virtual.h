#ifndef MONSOON_OBJPIPE_DETAIL_VIRTUAL_H
#define MONSOON_OBJPIPE_DETAIL_VIRTUAL_H

///\file
///\ingroup objpipe_detail

#include <cassert>
#include <type_traits>
#include <memory>
#include <monsoon/objpipe/errc.h>
#include <monsoon/objpipe/detail/adapt.h>
#include <monsoon/objpipe/detail/transport.h>

namespace monsoon::objpipe::detail {


///\brief Internal interface to virtualize an objpipe.
///\ingroup objpipe_detail
template<typename T>
class virtual_intf {
 public:
  virtual ~virtual_intf() noexcept {}

  virtual auto is_pullable() noexcept -> bool = 0;
  virtual auto wait() -> objpipe_errc = 0;
  virtual auto front() -> transport<T> = 0;
  virtual auto pop_front() -> objpipe_errc = 0;
  virtual auto pull() -> transport<T> = 0;
  virtual auto try_pull() -> transport<T> = 0;
};

///\brief Internal implementation to virtualize an objpipe.
///\ingroup objpipe_detail
template<typename Source>
class virtual_impl
: public virtual_intf<adapt::value_type<Source>>
{
 public:
  explicit virtual_impl(Source&& src)
  noexcept(std::is_nothrow_move_constructible_v<Source>)
  : src_(std::move(src))
  {}

  ~virtual_impl() noexcept override {}

  auto is_pullable()
  noexcept
  -> bool override {
    return src_.is_pullable();
  }

  auto wait()
  -> objpipe_errc override {
    return src_.wait();
  }

  auto front()
  -> transport<adapt::value_type<Source>> override {
    return src_.front();
  }

  auto pop_front()
  -> objpipe_errc override {
    return src_.pop_front();
  }

  auto try_pull()
  -> transport<adapt::value_type<Source>> override {
    return adapt::raw_try_pull(src_);
  }

  auto pull()
  -> transport<adapt::value_type<Source>> override {
    return adapt::raw_pull(src_);
  }

 private:
  Source src_;
};

/**
 * \brief An objpipe that hides the source behind an interface.
 * \ingroup objpipe_detail
 *
 * \details
 * The virtual_pipe hides an objpipe behind an interface.
 * It is used by the reader type to provide a uniform boundary for functions.
 *
 * \tparam T The type of elements iterated by the pipe.
 */
template<typename T>
class virtual_pipe {
 public:
  constexpr virtual_pipe() = default;

  template<typename Source>
  explicit virtual_pipe(Source&& src)
  : pimpl_(std::make_unique<virtual_impl<std::decay_t<Source>>>(std::forward<Source>(src)))
  {}

  auto is_pullable() noexcept
  -> bool {
    assert(pimpl_ != nullptr);
    return pimpl_->is_pullable();
  }

  auto wait()
  -> objpipe_errc {
    assert(pimpl_ != nullptr);
    return pimpl_->wait();
  }

  auto front()
  -> transport<T> {
    assert(pimpl_ != nullptr);
    return pimpl_->front();
  }

  auto pop_front()
  -> objpipe_errc {
    assert(pimpl_ != nullptr);
    return pimpl_->pop_front();
  }

  auto try_pull()
  -> transport<T> {
    assert(pimpl_ != nullptr);
    return pimpl_->try_pull();
  }

  auto pull()
  -> transport<T> {
    assert(pimpl_ != nullptr);
    return pimpl_->pull();
  }

 private:
  std::unique_ptr<virtual_intf<T>> pimpl_;
};


} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_VIRTUAL_H */
