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

  virtual ~cache_intf() noexcept {}

  virtual auto get_if_present(const key_type& key)
  -> pointer = 0;

  virtual auto get(const key_type& key)
  -> pointer = 0;
};

template<typename T, typename U>
class cache {
  // Cache builder will use private constructor to instantiate cache.
  template<typename K, typename V, typename Hash, typename Eq, typename Alloc>
  friend class cache_builder;

 public:
  using key_type = typename cache_intf<T, U>::key_type;
  using pointer = typename cache_intf<T, U>::pointer;

  cache() = delete; // Use builder.

  static constexpr auto builder()
  -> cache_builder<T, U> {
    return cache_builder<T, U>();
  }

  template<typename Alloc>
  static constexpr auto builder(Alloc alloc)
  -> cache_builder<T, U, std::hash<T>, std::equal_to<T>, Alloc> {
    return cache_builder<T, U, std::hash<T>, std::equal_to<T>, Alloc>(
        std::hash<T>(), std::equal_to<T>(), std::move(alloc));
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
  std::shared_ptr<cache_intf<T, U>> impl_;
};


} /* namespace monsoon::cache */

#endif /* MONSOOON_CACHE_H */
