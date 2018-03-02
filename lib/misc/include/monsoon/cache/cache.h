#ifndef MONSOOON_CACHE_H
#define MONSOOON_CACHE_H

#include <memory>
#include <type_traits>
#include <monsoon/cache/builder.h>

namespace monsoon::cache {


template<typename T, typename U>
class cache_intf {
 public:
  using key_type = T;
  using pointer = std::shared_ptr<U>;

  virtual auto get_if_present(const key_type& key) const
  -> pointer;

  virtual auto get(const key_type& key)
  -> pointer;
};

template<typename T, typename U>
class cache {
 public:
  using key_type = typename cache_impl<T, U>::key_type;
  using pointer = typename cache_impl<T, U>::pointer;

  cache() = delete; // Use builder.

  static constexpr auto builder()
  -> cache_builder {
    return cache_builder();
  }

  static constexpr auto builder(Alloc alloc)
  -> cache_builder {
    return cache_builder(std::move(alloc));
  }

 private:
  explicit cache(std::shared_ptr<cache_intf<T, U>>&& impl) noexcept
  : impl_(std::move(impl))
  {}

 public:
  auto get_if_present(const key_type& key) const {
    return impl_->get_if_present(key);
  }

  auto get(const key_type& key) const {
    return impl_->get(key);
  }

  auto operator()(const key_type& key) const {
    return get(key);
  }

 private:
  std::shared_ptr<cache_intf<T>> impl_;
};


} /* namespace monsoon::cache */

namespace monsoon {


using monsoon::cache;


} /* namespace monsoon */

#endif /* MONSOOON_CACHE_H */
