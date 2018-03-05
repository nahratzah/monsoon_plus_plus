#ifndef MONSOON_CACHE_ACCESS_EXPIRE_DECORATOR_H
#define MONSOON_CACHE_ACCESS_EXPIRE_DECORATOR_H

///\file
///\ingroup cache_detail

#include <chrono>
#include <memory>

namespace monsoon::cache {


/**
 * \brief Cache decorator that handles access expire.
 * \ingroup cache_detail
 *
 * \bug While the access expire correctly suppresses the element
 * once the timer is up, it would be nice to use a list of elements
 * to expire that can be updated independent of the bucket.
 * Currently, elements remain allocated until the bucket cleanup happens.
 */
struct access_expire_decorator {
  using clock_type = std::chrono::steady_clock;
  using time_point = std::chrono::time_point<clock_type>;

  struct access_init {
    time_point expire;
  };

  ///\brief Element decorator counterpart.
  class element_decorator_type {
    friend struct access_expire_decorator;

   public:
    template<typename Alloc, typename... Types>
    element_decorator_type(
        [[maybe_unused]] std::allocator_arg_t aa,
        [[maybe_unused]] Alloc a,
        const std::tuple<Types...>& init)
    : access_expire_(std::get<access_init>(init).expire)
    {}

    bool is_expired() const noexcept {
      return clock_type::now() > access_expire_;
    }

   private:
    time_point access_expire_;
  };

  ///\brief Implementation of access_expire_decorator for a given cache.
  template<typename ImplType>
  struct for_impl_type {
    using element_decorator_type = access_expire_decorator::element_decorator_type;

    for_impl_type(const cache_builder_vars& b)
    : duration(b.access_expire().value())
    {}

    auto init_tuple() const
    noexcept
    -> std::tuple<access_init> {
      return std::make_tuple(access_init{ clock_type::now() + duration });
    }

    auto on_hit(element_decorator_type& elem) const noexcept -> void {
      elem.access_expire_ = clock_type::now() + duration;
    }

    std::chrono::seconds duration;
  };
};


} /* namespace monsoon::cache */

#endif /* MONSOON_CACHE_ACCESS_EXPIRE_DECORATOR_H */
