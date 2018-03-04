#ifndef MONSOON_CACHE_ACCESS_EXPIRE_DECORATOR_H
#define MONSOON_CACHE_ACCESS_EXPIRE_DECORATOR_H

///\file
///\ingroup cache

#include <chrono>
#include <memory>

namespace monsoon::cache {


/**
 * \brief Cache decorator that handles access expire.
 * \ingroup cache
 *
 * \bug While the access expire correctly suppresses the element
 * once the timer is up, it would be nice to use a list of elements
 * to expire that can be updated independent of the bucket.
 * Currently, elements remain allocated until the bucket cleanup happens.
 */
struct access_expire_decorator {
  using clock_type = std::chrono::steady_clock;
  using time_point = std::chrono::time_point<clock_type>;

  access_expire_decorator(const cache_builder_vars& b)
  : duration(b.access_expire().value())
  {}

  auto init_tuple() const
  noexcept
  -> std::tuple<access_expire_decorator> {
    return std::make_tuple(*this);
  }

  ///\brief Element decorator counterpart.
  class element_decorator_type {
    friend struct access_expire_decorator;

   public:
    template<typename Alloc, typename... Types>
    element_decorator_type(
        [[maybe_unused]] std::allocator_arg_t aa,
        [[maybe_unused]] Alloc a,
        const std::tuple<Types...>& init)
    : access_expire_(clock_type::now()
        + std::get<access_expire_decorator>(init).duration)
    {}

    bool is_expired() const noexcept {
      return clock_type::now() > access_expire_;
    }

   private:
    time_point access_expire_;
  };

  auto on_hit(element_decorator_type& elem) const noexcept -> void {
    elem.access_expire_ = clock_type::now() + duration;
  }

  std::chrono::seconds duration;
};


} /* namespace monsoon::cache */

#endif /* MONSOON_CACHE_ACCESS_EXPIRE_DECORATOR_H */
