#ifndef MONSOOON_CACHE_H
#define MONSOOON_CACHE_H

#include <functional>
#include <memory>
#include <type_traits>
#include <monsoon/cache/builder.h>
#include <monsoon/cache/key_decorator.h>
#include <monsoon/cache/cache_query.h>

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

template<typename T, typename U, typename Hash, typename Eq, typename Alloc, typename Create>
class extended_cache_intf
: public cache_intf<T, U>
{
 public:
  using pointer = typename cache_intf<T, U>::pointer;
  using create_result_type =
      std::decay_t<decltype(std::declval<const Create&>()(std::declval<Alloc&>(), std::declval<const T&>()))>;
  using extended_query_type = cache_query<
      std::function<bool(const key_decorator<T>&)>,
      std::function<std::tuple<T>()>,
      std::function<create_result_type(Alloc)>>;

  template<typename HashArg, typename EqArg, typename CreateArg>
  extended_cache_intf(
      HashArg&& hash,
      EqArg&& eq,
      CreateArg&& create)
  : hash(std::forward<HashArg>(hash)),
    eq(std::forward<EqArg>(eq)),
    create(std::forward<CreateArg>(create))
  {}

  ~extended_cache_intf() noexcept override {}

  using cache_intf<T, U>::get_if_present;
  using cache_intf<T, U>::get;

  virtual auto get(const extended_query_type& q) -> pointer = 0;

  Hash hash;
  Eq eq;
  Create create;
};

template<typename T, typename U>
class cache {
  // Extended cache calls our constructor.
  template<typename OtherT, typename OtherU, typename Hash, typename Eq, typename Alloc, typename Create>
  friend class extended_cache;

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
  auto get_if_present(const key_type& key) const
  -> pointer {
    return impl_->get_if_present(key);
  }

  auto get(const key_type& key) const
  -> pointer {
    return impl_->get(key);
  }

  auto operator()(const key_type& key) const
  -> pointer {
    return get(key);
  }

 private:
  std::shared_ptr<cache_intf<T, U>> impl_;
};

template<typename T, typename U, typename Hash, typename Eq, typename Alloc, typename Create>
class extended_cache
: public cache<T, U>
{
  // Cache builder will use private constructor to instantiate cache.
  template<typename OtherT, typename OtherU, typename OtherHash, typename OtherEq, typename OtherAlloc>
  friend class cache_builder;

 private:
  using pointer = typename cache<T, U>::pointer;
  using extended_cache_type = extended_cache_intf<T, U, Hash, Eq, Alloc, Create>;
  using extended_query_type = typename extended_cache_type::extended_query_type;

  extended_cache(std::shared_ptr<extended_cache_type>&& impl) noexcept
  : cache<T, U>(impl),
    impl_(std::move(impl))
  {}

 public:
  using cache<T, U>::get_if_present;
  using cache<T, U>::get;
  using cache<T, U>::operator();

  template<typename... Args>
  auto get(const Args&... args) const
  -> pointer {
    return impl_->lookup_or_create(
        extended_query_type(
            impl_->hash_(args...),
            [this, &args...](const key_decorator<T>& s) { return impl_->eq_(s.key, args...); },
            [this, &args...]() { return std::make_tuple(T(args...)); },
            [this, &args...](Alloc alloc) { return impl_->create_(alloc, args...); }));
  }

 private:
  std::shared_ptr<extended_cache_type> impl_;
};


} /* namespace monsoon::cache */

#endif /* MONSOOON_CACHE_H */
