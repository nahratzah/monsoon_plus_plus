#ifndef MONSOOON_CACHE_H
#define MONSOOON_CACHE_H

///\file
///\ingroup cache

#include <functional>
#include <memory>
#include <type_traits>
#include <monsoon/cache/builder.h>
#include <monsoon/cache/key_decorator.h>
#include <monsoon/cache/cache_query.h>

namespace monsoon::cache {


/**
 * \brief Simple key-value interface of cache.
 * \ingroup cache_detail
 * \details The simple interface omits the variable arguments interface
 * of the \ref extended_cache_intf "extended cache".
 * \tparam K The key type of the cache.
 * \tparam V The mapped type of the cache.
 * \sa \ref cache<K,V>
 */
template<typename K, typename V>
class cache_intf {
 public:
  using key_type = K;
  using pointer = std::shared_ptr<V>;

  virtual ~cache_intf() noexcept {}

  virtual auto get_if_present(const key_type& key)
  -> pointer = 0;

  virtual auto get(const key_type& key)
  -> pointer = 0;
};

/**
 * \brief Extended cache interface.
 * \ingroup cache_detail
 * \details The extended interface allows for querying a cache using a set
 * of arguments.
 * \tparam K The key type of the cache.
 * \tparam V The mapped type of the cache.
 * \tparam Hash A hash function for the key.
 * \tparam Eq An equality predicate for the key.
 * \tparam Alloc Allocator used by the cache.
 * \tparam Create Mapped type construction function.
 * \sa \ref extended_cache<K,V>
 */
template<typename K, typename V, typename Hash, typename Eq, typename Alloc, typename Create>
class extended_cache_intf
: public cache_intf<K, V>
{
 public:
  using pointer = typename cache_intf<K, V>::pointer;
  using create_result_type =
      std::decay_t<decltype(std::declval<const Create&>()(std::declval<Alloc&>(), std::declval<const K&>()))>;
  using extended_query_type = cache_query<
      std::function<bool(const key_decorator<K>&)>,
      std::function<std::tuple<K>()>,
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

  using cache_intf<K, V>::get_if_present;
  using cache_intf<K, V>::get;

  virtual auto get(const extended_query_type& q) -> pointer = 0;

  Hash hash;
  Eq eq;
  Create create;
};

/**
 * \brief A key-value cache.
 * \ingroup cache
 * \details This cache allows looking up values, by a given key.
 * \tparam K The key type of the cache.
 * \tparam V The mapped type of the cache.
 */
template<typename K, typename V>
class cache {
  // Extended cache calls our constructor.
  template<typename OtherK, typename OtherV, typename Hash, typename Eq, typename Alloc, typename Create>
  friend class extended_cache;

 public:
  using key_type = typename cache_intf<K, V>::key_type;
  using pointer = typename cache_intf<K, V>::pointer;

  cache() = delete; // Use builder.

  static constexpr auto builder()
  -> cache_builder<K, V> {
    return cache_builder<K, V>();
  }

  template<typename Alloc>
  static constexpr auto builder(Alloc alloc)
  -> cache_builder<K, V, std::hash<K>, std::equal_to<K>, Alloc> {
    return cache_builder<K, V, std::hash<K>, std::equal_to<K>, Alloc>(
        std::hash<K>(), std::equal_to<K>(), std::move(alloc));
  }

 private:
  explicit cache(std::shared_ptr<cache_intf<K, V>>&& impl) noexcept
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
  std::shared_ptr<cache_intf<K, V>> impl_;
};

/**
 * \brief A cache that allows lookup by argument pack.
 * \ingroup cache
 * \details This cache allows looking up values, by a given argument pack.
 * \tparam K The key type of the cache.
 * \tparam V The mapped type of the cache.
 * \tparam Hash A hash function for the key.
 * \tparam Eq An equality predicate for the key.
 * \tparam Alloc Allocator used by the cache.
 * \tparam Create Mapped type construction function.
 */
template<typename K, typename V, typename Hash, typename Eq, typename Alloc, typename Create>
class extended_cache {
  // Cache builder will use private constructor to instantiate cache.
  template<typename OtherK, typename OtherV, typename OtherHash, typename OtherEq, typename OtherAlloc>
  friend class cache_builder;

 private:
  using extended_cache_type = extended_cache_intf<K, V, Hash, Eq, Alloc, Create>;
  using extended_query_type = typename extended_cache_type::extended_query_type;

 public:
  using key_type = typename cache<K, V>::key_type;
  using pointer = typename cache<K, V>::pointer;

  extended_cache() = delete; // Use builder.

  static constexpr auto builder()
  -> cache_builder<K, V, Hash, Eq, Alloc> {
    return cache_builder<K, V, Hash, Eq, Alloc>();
  }

  static constexpr auto builder(Alloc alloc)
  -> cache_builder<K, V, Hash, Eq, Alloc> {
    return cache_builder<K, V, Hash, Eq, Alloc>(
        Hash(), Eq(), std::move(alloc));
  }

 private:
  extended_cache(std::shared_ptr<extended_cache_type>&& impl) noexcept
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

  template<typename... Args>
  auto get(Args&&... args) const
  -> pointer {
    return impl_->lookup_or_create(
        extended_query_type(
            impl_->hash_(std::as_const(args)...),
            [this, &args...](const key_decorator<K>& s) { return impl_->eq_(s.key, std::as_const(args)...); },
            [this, &args...]() { return std::make_tuple(K(std::as_const(args)...)); },
            [this, &args...](Alloc alloc) { return impl_->create_(alloc, std::forward<Args>(args)...); }));
  }

  operator cache<K, V>() const & {
    return cache<K, V>(impl_);
  }

  operator cache<K, V>() && {
    return cache<K, V>(std::move(impl_));
  }

 private:
  std::shared_ptr<extended_cache_type> impl_;
};


} /* namespace monsoon::cache */

#endif /* MONSOOON_CACHE_H */
