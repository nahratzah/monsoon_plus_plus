#ifndef MONSOON_CACHE_MAX_AGE_DECORATOR_H
#define MONSOON_CACHE_MAX_AGE_DECORATOR_H

///\file
///\ingroup cache

#include <chrono>
#include <memory>

namespace monsoon::cache {


/**
 * \brief Decorator that enforces the max_age property.
 */
struct max_age_decorator {
  using clock_type = std::chrono::steady_clock;
  using time_point = std::chrono::time_point<clock_type>;

  max_age_decorator(const cache_builder_vars& b)
  : duration(b.access_expire().value())
  {}

  auto init_tuple() const
  noexcept
  -> std::tuple<max_age_decorator> {
    return std::make_tuple(*this);
  }

  ///\brief Element decorator counterpart.
  class element_decorator_type {
   public:
    template<typename Alloc, typename... Types>
    element_decorator_type(
        [[maybe_unused]] std::allocator_arg_t aa,
        [[maybe_unused]] Alloc a,
        const std::tuple<Types...>& init)
    : max_age_expire_(clock_type::now()
        + std::get<max_age_decorator>(init).duration)
    {}

    bool is_expired() const noexcept {
      return clock_type::now() > max_age_expire_;
    }

   private:
    time_point max_age_expire_;
  };

  std::chrono::seconds duration;
};


} /* namespace monsoon::cache */

#endif /* MONSOON_CACHE_MAX_AGE_DECORATOR_H */
