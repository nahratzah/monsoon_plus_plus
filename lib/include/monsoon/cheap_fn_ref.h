#ifndef MONSOON_CHEAP_FN_REF_H
#define MONSOON_CHEAP_FN_REF_H

#include <cassert>
#include <functional>
#include <utility>

namespace monsoon {


template<typename = void()> class cheap_fn_ref;


/**
 * \brief Reference to a functor.
 * \details
 * This is designed to be a lot cheaper than std::function.
 *
 * \note
 * This class holds a reference to the functor it was constructed with.
 * Invoking the function requires that the original functor remains valid.
 *
 * \tparam R Return type of the functor.
 */
template<typename R, typename... Args>
class cheap_fn_ref<R(Args...)> {
  public:
  using result_type = R;

  ///\brief Default constructor.
  constexpr cheap_fn_ref() noexcept = default;
  cheap_fn_ref(const cheap_fn_ref&) = default;
  cheap_fn_ref& operator=(const cheap_fn_ref&) = default;

  ///\brief Move constructor.
  cheap_fn_ref(cheap_fn_ref&& y) noexcept
  : fn_(std::exchange(y.fn_, nullptr)),
    datum_(std::exchange(y.datum_, nullptr))
  {}

  ///\brief Move assignment.
  auto operator=(cheap_fn_ref&& y) noexcept -> cheap_fn_ref& {
    fn_ = std::exchange(y.fn_, nullptr);
    datum_ = std::exchange(y.datum_, nullptr);
  }

  template<typename U>
  cheap_fn_ref(const cheap_fn_ref<U>&) = delete;
  template<typename U>
  cheap_fn_ref& operator=(const cheap_fn_ref<U>&) = delete;
  template<typename U>
  cheap_fn_ref(cheap_fn_ref<U>&&) = delete;
  template<typename U>
  cheap_fn_ref& operator=(cheap_fn_ref<U>&&) = delete;

  /**
   * \brief Constructor from functor.
   * \details
   * Wraps the \p functor so it can be invoked using this reference.
   *
   * \param functor The functor to wrap.
   */
  template<typename Functor>
  cheap_fn_ref(Functor&& functor) noexcept {
    fn_ = [](void* ptr, Args... args) -> R {
      assert(ptr != nullptr);
      return std::invoke(*static_cast<std::remove_reference_t<Functor>*>(ptr), std::forward<Args>(args)...);
    };

    if constexpr(std::is_const_v<std::remove_reference_t<Functor>>) {
      datum_ = const_cast<void*>(static_cast<const void*>(std::addressof(functor)));
    } else { // Not const.
      datum_ = static_cast<void*>(std::addressof(functor));
    }
  }

  ///\brief Invoke the functor.
  ///\return The return value of the functor.
  auto operator()(Args... args) const -> R {
    assert(fn_ != nullptr);
    return std::invoke(fn_, datum_, std::forward<Args>(args)...);
  }

  ///\brief Test if the reference is invocable.
  ///\note This method doesn't test if the reference is valid, only if it is set.
  ///\return True if the functor is invocable.
  explicit operator bool() const noexcept {
    return fn_ != nullptr;
  }

  ///\brief Test if the reference is not invocable.
  ///\return True if the functor is not invocable.
  auto operator!() const noexcept -> bool {
    return fn_ == nullptr;
  }

  private:
  ///\brief Functor that does the unwrapping and invoking of the functor.
  R (*fn_)(void*, Args...) = nullptr;
  ///\brief Type-erased value of the functor.
  void *datum_ = nullptr;
};


} /* namespace monsoon */

#endif /* MONSOON_CHEAP_FN_REF_H */
