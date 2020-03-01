#ifndef MONSOON_UNIQUE_ALLOC_PTR_H
#define MONSOON_UNIQUE_ALLOC_PTR_H

#include <memory>
#include <type_traits>

#if __cplusplus <= 201703L
# include <exception>
#endif

namespace monsoon::tx::detail {


///\brief A deleter for use with std::unique_ptr, that uses an allocator.
///\tparam T The type for which this deleter exists.
///\tparam Alloc The allocator type for this deleter.
template<typename T, typename Alloc>
class deleter_with_alloc {
  public:
  using allocator_type = Alloc;
  using allocator_traits = std::allocator_traits<allocator_type>;

  static_assert(std::is_same_v<typename allocator_traits::value_type, T>,
      "allocator must match element type");

  deleter_with_alloc() noexcept(std::is_nothrow_default_constructible_v<Alloc>) = default;

  explicit deleter_with_alloc(Alloc&& alloc)
      noexcept(std::is_nothrow_move_constructible_v<Alloc>)
  : alloc_(std::move(alloc))
  {}

  explicit deleter_with_alloc(const Alloc& alloc)
      noexcept(std::is_nothrow_copy_constructible_v<Alloc>)
  : alloc_(alloc)
  {}

  void operator()(T* ptr) {
    allocator_traits traits;
    traits.destroy(alloc_, ptr);
    traits.deallocate(alloc_, ptr, 1);
  }

  auto get_allocator() noexcept -> Alloc& { return alloc_; }
  auto get_allocator() const noexcept -> const Alloc& { return alloc_; }

  private:
  Alloc alloc_;
};


///\brief Unique pointer with an allocator.
template<typename T, typename Alloc>
using unique_alloc_ptr = std::unique_ptr<
    T,
    deleter_with_alloc<T, Alloc>>;


///\brief Allocate a unique pointer using the given allocator.
///\param alloc The allocator to allocate and deallocate the object with.
///\param args The arguments passed to the constructor of the type.
///\note The allocator is used to construct the object, so for instance scoped-allocator will propagate itself if possible.
template<typename T, typename Alloc, typename... Args>
auto allocate_unique(Alloc&& alloc, Args&&... args)
-> unique_alloc_ptr<T, std::decay_t<Alloc>> {
  using d_type = typename unique_alloc_ptr<T, std::decay_t<Alloc>>::deleter_type;

  d_type d = d_type(std::forward<Alloc>(alloc));
  typename d_type::allocator_traits alloc_traits;

  T* ptr = alloc_traits.allocate(d.get_allocator(), 1);
  try {
    alloc_traits.construct(d.get_allocator(), ptr, std::forward<Args>(args)...);
  } catch (...) {
    // Prevent resource leak.
    try {
      alloc_traits.deallocate(d.get_allocator(), ptr, 1);
    } catch (...) {
#if __cplusplus <=  201402L
      std::unexpected();
#else
      // Swallowing the exception and leaking resource.
#endif
    }
    throw;
  }

  return unique_alloc_ptr<T, std::decay_t<Alloc>>(ptr, std::move(d));
};


} /* namespace monsoon::tx::detail */

#endif /* MONSOON_UNIQUE_ALLOC_PTR_H */
