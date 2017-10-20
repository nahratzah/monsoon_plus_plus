#ifndef MONSOON_OBJPIPE_DETAIL_CALLBACKED_H
#define MONSOON_OBJPIPE_DETAIL_CALLBACKED_H

///@file monsoon/objpipe/detail/callbacked.h <monsoon/objpipe/detail/callbacked.h>

#include <functional>
#include <boost/coroutines2/protected_fixedsize_stack.hpp>
#include <boost/coroutines2/coroutine.hpp>

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
  using value_type = T;
  ///@copydoc reader_intf<T>::pointer
  using pointer = std::add_pointer<value_type>;

 private:
  using co_type = boost::coroutines2::coroutine<pointer>;

 public:
  template<typename Fn>
  explicit callbacked(Fn&& fn)
  : co_fn_(
      std::bind(
          [](auto fn, co_type::push_type& yield) {
            std::invoke(
                fn,
                [&yield](auto&& v) {
                  yield(&v);
                });
          },
          std::forward<Fn>(fn),
          std::placeholders::_1))
  {}

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
    return ofered_ == nullptr;
  }

  ///@copydoc reader_intf<T>::pull(objpipe_errc&)
  auto pull(objpipe_errc& e) -> std::optional<value_type> {
    ensure_populated_();
    if (offered_ == nullptr) {
      e = objpipe_errc::closed;
      return {};
    }
    return std::move(*std::exchange(offered_, nullptr));
  }

  ///@copydoc reader_intf<T>::pull()
  auto pull() -> value_type {
    ensure_populated_();
    if (offered_ == nullptr)
      throw std::system_error(objpipe_errc::closed, objpipe_category());
    return std::move(*std::exchange(offered_, nullptr));
  }

  ///@copydoc reader_intf<T>::front()
  auto front() const -> std::variant<pointer, objpipe_errc> {
    ensure_populated_();
    if (offered_ == nullptr)
      return { std::in_place_index<1>, objpipe_errc::closed };
    return { std::in_place_index<0>, offered_ };
  }

  ///@copydoc reader_intf<T>::pop_front()
  auto pop_front() -> objpipe_errc {
    ensure_populated_();
    if (offered_ == nullptr) return objpipe_errc::closed;
    offered_ = nullptr;
    return objpipe_errc::success;
  }

  /**
   * @copydoc reader_intf<T>::add_continuation(std::unique_ptr<continuation_intf,writer_release>&&)
   * @note The callback objpipe holds no continuations, as it cannot transition from \ref empty() to !\ref empty().
   */
  void add_continuation(std::unique_ptr<continuation_intf, writer_release>&& c) {
    /* SKIP */
  }

  ///@copydoc reader_intf<T>::add_continuation(std::unique_ptr<continuation_intf,writer_release>&&)
  void erase_continuation(continuation_intf* c) {
    /* SKIP */
  }

 private:
  void on_last_reader_gone_() noexcept override {
    /* SKIP: co routine destruction is handled in destructor. */
  }

  void on_last_writer_gone_() noexcept override {
    assert(false); // Should never be called.
  }

  /**
   * @brief Ensure next value of offered_ is available.
   * @note This lazily initializes the coroutine the first time it is called.
   */
  void ensure_populated_() const {
    if (offered_ != nullptr) return;

    bool need_advance = true;
    std::call_once(
        co_init_once_,
        [this, &need_advance]() {
          need_advance = false;
          co_ = co_type::pull_type(
              boost::coroutines2::protected_fixedsize_stack(),
              std::move(co_fn_));
        });
    if (need_advance && co_) co_();
    if (co_) offered_ = co_.get();
  }

  mutable pointer offered_ = nullptr;
  mutable co_type::pull_type co_;
  mutable std::function<void(co_type::push_type&)> co_fn_;
  mutable std::once_flag co_init_once_;
};


}}} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_CALLBACKED_H */
