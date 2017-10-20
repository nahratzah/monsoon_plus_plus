#ifndef MONSOON_OBJPIPE_DETAIL_CALLBACKED_H
#define MONSOON_OBJPIPE_DETAIL_CALLBACKED_H

///@file monsoon/objpipe/detail/callbacked.h <monsoon/objpipe/detail/callbacked.h>

#include <monsoon/objpipe/detail/reader_intf.h>
#include <functional>
#include <boost/coroutine2/protected_fixedsize_stack.hpp>
#include <boost/coroutine2/coroutine.hpp>

namespace monsoon {
namespace objpipe {
namespace detail {


/**
 * @brief A obj pipe that is supplied by a callback invocation.
 *
 * This implementation uses boost::coroutines2 to handle the callback.
 * The coroutine will not be constructed until the first call to a reader-based function.
 * @tparam T @parblock
 *   The type of object emitted by the object pipe.
 *   @note The type may not be a reference, nor may it be const.
 * @endparblock
 * @sa @ref monsoon::objpipe::new_callback()
 * @headerfile monsoon/objpipe/detail/callbacked.h <monsoon/objpipe/detail/callbacked.h>
 */
template<typename T>
class callbacked final
: public reader_intf<T>
{
  static_assert(!std::is_reference_v<T>,
      "Callbacked objpipe cannot deal with references.");
  static_assert(!std::is_const_v<T>,
      "Callbacked objpipe cannot deal with const objects.");

 public:
  ///@copydoc reader_intf<T>::value_type
  using value_type = typename reader_intf<T>::value_type;
  ///@copydoc reader_intf<T>::pointer
  using pointer = typename reader_intf<T>::pointer;

 private:
  using co_type = boost::coroutines2::coroutine<pointer>;

 public:
  template<typename Fn>
  explicit callbacked(Fn&& fn)
  : co_(
      boost::coroutines2::protected_fixedsize_stack(),
      fn_wrapper_<Fn>(std::forward<Fn>(fn)))
  {
    offered_ = co_.get();
    assert(offered_ == nullptr);
  }

  ///@copydoc reader_intf<T>::is_pullable()
  auto is_pullable() const noexcept -> bool override {
    ensure_populated_();
    return offered_ != nullptr;
  }

  ///@copydoc reader_intf<T>::wait()
  auto wait() const noexcept -> objpipe_errc override {
    ensure_populated_();
    return (offered_ == nullptr
        ? objpipe_errc::closed
        : objpipe_errc::success);
  }

  ///@copydoc reader_intf<T>::empty()
  auto empty() const noexcept -> bool override {
    ensure_populated_();
    return offered_ == nullptr;
  }

  ///@copydoc reader_intf<T>::pull(objpipe_errc&)
  auto pull(objpipe_errc& e) -> std::optional<value_type> override {
    ensure_populated_();
    if (offered_ == nullptr) {
      e = objpipe_errc::closed;
      return {};
    }
    return std::move(*std::exchange(offered_, nullptr));
  }

  ///@copydoc reader_intf<T>::pull()
  auto pull() -> value_type override {
    ensure_populated_();
    if (offered_ == nullptr)
      throw std::system_error(static_cast<int>(objpipe_errc::closed), objpipe_category());
    return std::move(*std::exchange(offered_, nullptr));
  }

  ///@copydoc reader_intf<T>::front()
  auto front() const -> std::variant<pointer, objpipe_errc> override {
    using result_type = std::variant<pointer, objpipe_errc>;

    ensure_populated_();
    if (offered_ == nullptr)
      return result_type(std::in_place_index<1>, objpipe_errc::closed);
    return result_type(std::in_place_index<0>, offered_);
  }

  ///@copydoc reader_intf<T>::pop_front()
  auto pop_front() -> objpipe_errc override {
    ensure_populated_();
    if (offered_ == nullptr) return objpipe_errc::closed;
    offered_ = nullptr;
    return objpipe_errc::success;
  }

  /**
   * @copydoc reader_intf<T>::add_continuation(std::unique_ptr<continuation_intf,writer_release>&&)
   * @note The callback objpipe holds no continuations, as it cannot transition from \ref empty() to !\ref empty().
   */
  void add_continuation(std::unique_ptr<continuation_intf, writer_release>&& c) override {
    /* SKIP */
  }

  ///@copydoc reader_intf<T>::add_continuation(std::unique_ptr<continuation_intf,writer_release>&&)
  void erase_continuation(continuation_intf* c) override {
    /* SKIP */
  }

 private:
  void on_last_reader_gone_() noexcept override {
    /* SKIP: co routine destruction is handled in destructor. */
  }

  void on_last_writer_gone_() noexcept override {
    assert(false); // Should never be called.
  }

  struct yield_wrapper_ {
    typename co_type::push_type& yield;

    yield_wrapper_(typename co_type::push_type& yield) : yield(yield) {}

    void operator()(value_type&& v) const {
      yield(&v);
    }

    void operator()(const value_type& v) const {
      (*this)(value_type(v));
    }
  };

  template<typename Fn>
  struct fn_wrapper_ {
    std::decay_t<Fn> fn_;

    fn_wrapper_(Fn&& fn) : fn_(std::forward<Fn>(fn)) {}

    void operator()(typename co_type::push_type& yield) {
      yield(nullptr); // Delay entering co routine.
      yield_wrapper_ wrapper = yield_wrapper_(yield);
      fn_(wrapper);
    }
  };

  /**
   * @brief Ensure next value of offered_ is available.
   * @note This lazily initializes the coroutine the first time it is called.
   */
  void ensure_populated_() const {
    if (offered_ != nullptr) return;

    if (co_) co_();
    if (co_) {
      offered_ = co_.get();
      assert(offered_ != nullptr);
    }
  }

  mutable pointer offered_ = nullptr;
  mutable typename co_type::pull_type co_;
};


}}} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_CALLBACKED_H */
