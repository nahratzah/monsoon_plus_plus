#ifndef MONSOON_CACHE_ALLOCATOR_H
#define MONSOON_CACHE_ALLOCATOR_H

///\file
///\ingroup cache

#include <cstddef>
#include <cstdint>
#include <memory>
#include <scoped_allocator>
#include <type_traits>

namespace monsoon::cache {


template<typename Allocator>
class cache_allocator;

/**
 * \brief Interface used by cache_allocator to record changes in memory usage.
 */
class cache_alloc_dealloc_observer {
 public:
  ///\brief Inform cache of memory having been allocated.
  ///\param[in] n The number of items.
  ///\param[in] sz The size of items.
  virtual void add_mem_use(std::uintptr_t n, std::uintptr_t sz) noexcept = 0;
  ///\brief Inform cache of memory having been deallocated.
  ///\param[in] n The number of items.
  ///\param[in] sz The size of items.
  virtual void subtract_mem_use(std::uintptr_t n, std::uintptr_t sz) noexcept = 0;

  template<typename Allocator>
  static void maybe_set_stats_(
      cache_allocator<Allocator>& ca,
      std::weak_ptr<cache_alloc_dealloc_observer> stats) noexcept {
    ca.stats_ = stats;
    maybe_set_stats_(ca.nested_);
  }

  template<typename Outer, typename... Inner>
  static void maybe_set_stats(
      std::scoped_allocator_adaptor<Outer, Inner...>& sa,
      std::weak_ptr<cache_alloc_dealloc_observer> stats) noexcept {
    maybe_set_stats_(sa.outer_allocator(), stats);
    if constexpr(sizeof...(Inner) > 0)
      maybe_set_stats_(sa.inner_allocator(), stats);
  }

  template<typename Alloc>
  static void maybe_set_stats(
      Alloc& alloc,
      const std::weak_ptr<cache_alloc_dealloc_observer>& stats,
      ...) noexcept
  {
    static_assert(!std::is_const_v<Alloc> && !std::is_volatile_v<Alloc> && !std::is_reference_v<Alloc>,
        "Overload selected for wrong reasons: const/volatile/reference.");
  }
};

/**
 * \brief Allocator wrapper that enables cache memory use tracking.
 *
 * \details
 * Cache allocator maintains an internal weak reference to the cache.
 * Whenever an allocation/deallocation occurs, this reference (if not nullptr)
 * is used to inform the cache of memory consumption changes.
 *
 * \bug Cache allocator currently maintains reference on copy/move.
 * This may be a problem if a collection is moved into or out of the cached
 * data type. Copied-into data may lose its cache tracking, while moved-out-of
 * data may falsely maintain cache memory pressure.
 * \bug Scoped allocator containing one or more cache allocators,
 * has the same reference to cache in each sub allocator, which
 * unnecessarily wastes memory.
 * \bug Cache allocator requires explicit initialization, making for example
 * std::vector<int, cache_allocator<>> constructor require explicit allocator
 * initialization.
 * It should instead be default initializable, provided the underlying allocator
 * is default initializable.
 */
template<typename Allocator>
class cache_allocator {
  template<typename OtherAllocator> friend class cache_allocator;

 private:
  using nested_allocator = Allocator;
  using nested_traits = std::allocator_traits<Allocator>;
  using stats_type = cache_alloc_dealloc_observer;

 public:
  using value_type = typename nested_traits::value_type;
  using pointer = typename nested_traits::pointer;
  using const_pointer = typename nested_traits::const_pointer;
  using void_pointer = typename nested_traits::void_pointer;
  using const_void_pointer = typename nested_traits::const_void_pointer;
  using difference_type = typename nested_traits::difference_type;
  using size_type = typename nested_traits::size_type;
  using propagate_on_container_copy_assignment = std::true_type;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap = std::true_type;

  template<typename T>
  struct rebind {
    using other = cache_allocator<typename nested_traits::template rebind_alloc<T>>;
  };

  template<typename Nested = nested_allocator,
      typename = std::enable_if_t<std::is_constructible_v<nested_allocator, Nested>>>
  explicit cache_allocator(std::weak_ptr<stats_type> stats, Nested&& nested = Nested())
  noexcept(std::is_nothrow_constructible_v<nested_allocator, Nested>)
  : stats_(std::move(stats)),
    nested_(std::forward<Nested>(nested))
  {
    stats_type::maybe_set_stats(nested_, stats_);
  }

  template<typename Nested = nested_allocator,
      typename = std::enable_if_t<std::is_constructible_v<nested_allocator, Nested>>>
  explicit cache_allocator([[maybe_unused]] std::nullptr_t stats, Nested&& nested = Nested())
  noexcept(std::is_nothrow_constructible_v<nested_allocator, Nested>)
  : stats_(),
    nested_(std::forward<Nested>(nested))
  {
    stats_type::maybe_set_stats(nested_, stats_);
  }

  template<typename U,
      typename = std::enable_if_t<std::is_constructible_v<
          nested_allocator,
          std::add_lvalue_reference_t<std::add_const_t<typename cache_allocator<U>::nested_allocator>>>>>
  cache_allocator(const cache_allocator<U>& other)
  noexcept(std::is_nothrow_constructible_v<
      nested_allocator,
      std::add_lvalue_reference_t<std::add_const_t<typename cache_allocator<U>::nested_allocator>>>)
  : cache_allocator(other.stats_, other.nested_)
  {}

  template<typename U,
      typename = std::enable_if_t<std::is_constructible_v<
          nested_allocator,
          std::add_rvalue_reference_t<typename cache_allocator<U>::nested_allocator>>>>
  cache_allocator(cache_allocator<U>&& other)
  noexcept(std::is_nothrow_constructible_v<
      nested_allocator,
      std::add_rvalue_reference_t<typename cache_allocator<U>::nested_allocator>>)
  : cache_allocator(std::move(other.stats_), std::move(other.nested_))
  {}

  operator nested_allocator&() {
    return nested_;
  }

  operator const nested_allocator&() const {
    return nested_;
  }

  template<typename U>
  auto operator==(const cache_allocator<U>& other) const
  noexcept(noexcept(std::declval<const nested_allocator&>() == std::declval<const typename cache_allocator<U>::nested_allocator&>()))
  -> bool {
    return nested_ == other.nested_
        && stats_.lock() == other.stats.lock();
  }

  template<typename U>
  auto operator!=(const cache_allocator<U>& other) const
  noexcept(noexcept(std::declval<const nested_allocator&>() == std::declval<const typename cache_allocator<U>::nested_allocator&>()))
  -> bool {
    return !(*this == other);
  }

  [[nodiscard]]
  auto allocate(size_type n)
  -> pointer {
    pointer p = nested_traits::allocate(nested_, n);
    if (p != nullptr) add_mem_use(n);
    return p;
  }

  [[nodiscard]]
  auto allocate(size_type n, const_void_pointer hint)
  -> pointer {
    pointer p = nested_traits::allocate(nested_, n, hint);
    if (p != nullptr) add_mem_use(n);
    return p;
  }

  auto deallocate(pointer p, size_type n)
  noexcept(
      noexcept(nested_traits::deallocate(
              std::declval<nested_allocator&>(),
              std::declval<pointer>(),
              std::declval<size_type>())))
  -> void {
    nested_traits::deallocate(nested_, p, n);
    if (p != nullptr) this->subtract_mem_use(n);
  }

 private:
  auto add_mem_use(size_type n)
  noexcept
  -> void {
    auto stats_ptr = stats_.lock();
    if (stats_ptr != nullptr)
      stats_ptr->add_mem_use(n, sizeof(value_type));
  }

  auto subtract_mem_use(size_type n)
  noexcept
  -> void {
    auto stats_ptr = stats_.lock();
    if (stats_ptr != nullptr)
      stats_ptr->subtract_mem_use(n, sizeof(value_type));
  }

  std::weak_ptr<stats_type> stats_;
  nested_allocator nested_;
};


} /* namespace monsoon::cache */

namespace monsoon {


using cache::cache_allocator;


} /* namespace monsoon */

#endif /* MONSOON_CACHE_ALLOCATOR_H */
