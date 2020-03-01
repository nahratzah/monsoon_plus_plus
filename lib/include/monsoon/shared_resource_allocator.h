#ifndef MONSOON_SHARED_RESOURCE_ALLOCATOR_H
#define MONSOON_SHARED_RESOURCE_ALLOCATOR_H

#include <cstddef>
#include <limits>
#include <memory>
#include <type_traits>

#if __has_include(<memory_resource>)
# include <memory_resource>
#else
# include <boost/container/pmr/memory_resource.hpp>
# include <boost/container/pmr/global_resource.hpp>
#endif

namespace monsoon {


#if __has_include(<memory_resource>)
namespace pmr = std::pmr;
#else
namespace pmr = boost::container::pmr;
#endif


namespace detail {

///\brief Deleter used for memory resources that should not be deleted.
struct no_pmr_delete_ {
  void operator()([[maybe_unused]] pmr::memory_resource* ptr) const {}
};

}


template<typename T>
class shared_resource_allocator {
  template<typename> friend class shared_resource_allocator;

  public:
  using value_type = T;

  shared_resource_allocator()
  : mr_(pmr::get_default_resource(), detail::no_pmr_delete_())
  {}

  template<typename U>
  shared_resource_allocator(const shared_resource_allocator<U>& y) noexcept
  : mr_(y.mr_)
  {}

  explicit shared_resource_allocator(std::shared_ptr<pmr::memory_resource> mr)
  : mr_(mr)
  {}

  auto allocate(std::size_t n, const void* hint = 0) -> value_type* {
    if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
      throw std::bad_alloc();
    return static_cast<value_type*>(mr_->allocate(n * sizeof(T)));
  }

  auto deallocate(value_type* ptr, std::size_t n) -> void {
    mr_->deallocate(ptr, n * sizeof(T));
  }

  template<typename U>
  auto operator==(const shared_resource_allocator<U>& y) const noexcept -> bool {
    return mr_ == y.mr_;
  }

  template<typename U>
  auto operator!=(const shared_resource_allocator<U>& y) const noexcept -> bool {
    return !(*this == y);
  }

  private:
  std::shared_ptr<pmr::memory_resource> mr_;
};


template<>
class shared_resource_allocator<void> {
  template<typename> friend class shared_resource_allocator;

  public:
  using value_type = void;

  shared_resource_allocator()
  : mr_(pmr::get_default_resource(), detail::no_pmr_delete_())
  {}

  template<typename U>
  shared_resource_allocator(const shared_resource_allocator<U>& y) noexcept
  : mr_(y.mr_)
  {}

  explicit shared_resource_allocator(std::shared_ptr<pmr::memory_resource> mr)
  : mr_(mr)
  {}

  template<typename U>
  auto operator==(const shared_resource_allocator<U>& y) const noexcept -> bool {
    return mr_ == y.mr_;
  }

  template<typename U>
  auto operator!=(const shared_resource_allocator<U>& y) const noexcept -> bool {
    return !(*this == y);
  }

  private:
  std::shared_ptr<pmr::memory_resource> mr_;
};


} /* namespace monsoon */

#endif /* MONSOON_SHARED_RESOURCE_ALLOCATOR_H */
