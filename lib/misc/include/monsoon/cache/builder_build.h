#ifndef MONSOON_CACHE_BUILDER_BUILD_H
#define MONSOON_CACHE_BUILDER_BUILD_H

#include <memory>
#include <thread>
#include <type_traits>
#include <utility>
#include <monsoon/cache/builder.h>
#include <monsoon/cache/simple_cache_impl.h>
#include <monsoon/cache/thread_safe_decorator.h>
#include <monsoon/cache/weaken_decorator.h>
#include <monsoon/cache/access_expire_decorator.h>

namespace monsoon::cache {
namespace builder_detail {


template<typename T>
struct cache_key_decorator {
  using element_decorator_type = key_decorator<T>;

  template<typename Builder>
  constexpr cache_key_decorator([[maybe_unused]] const Builder& b) {}
};


namespace {


///\brief Keep track of all decorators that are to be applied to the cache.
///\tparam D All decorators that are to be passed to the cache.
template<typename... D>
struct cache_decorator_set {
  ///\brief Specialize the type of the cache.
  ///\tparam T The mapped type of the cache.
  ///\tparam Alloc The allocator for the cache.
  template<typename T, typename Alloc>
  using cache_type = simple_cache_impl<T, Alloc, D...>;
};


template<typename NextApply>
struct apply_thread_safe_ {
  template<typename... D>
  auto operator()(cache_decorator_set<D...> d)
  -> decltype(auto) {
    if (b.thread_safe())
      return next(cache_decorator_set<D..., thread_safe_decorator<true>>());
    else
      return next(cache_decorator_set<D..., thread_safe_decorator<false>>());
  }

  const cache_builder_vars& b;
  NextApply next;
};

template<typename NextApply>
auto apply_thread_safe(const cache_builder_vars& b, NextApply&& next)
-> apply_thread_safe_<std::decay_t<NextApply>> {
  return { b, std::forward<NextApply>(next) };
}


template<typename KeyType, typename NextApply>
struct apply_key_type_ {
  template<typename... D>
  auto operator()(cache_decorator_set<D...> d)
  -> decltype(auto) {
    if constexpr(std::is_same_v<void, KeyType>)
      return next(cache_decorator_set<D...>());
    else
      return next(cache_decorator_set<D..., cache_key_decorator<KeyType>>());
  }

  NextApply next;
};

template<typename Builder, typename NextApply>
auto apply_key_type(const Builder& b, NextApply&& next)
-> apply_key_type_<typename Builder::key_type, std::decay_t<NextApply>> {
  return { std::forward<NextApply>(next) };
}


template<typename NextApply>
struct apply_access_expire_ {
  template<typename... D>
  auto operator()(cache_decorator_set<D...> d)
  -> decltype(auto) {
    if (b.access_expire().has_value())
      return next(cache_decorator_set<D..., access_expire_decorator>());
    else
      return next(cache_decorator_set<D..., weaken_decorator>());
  }

  const cache_builder_vars& b;
  NextApply next;
};

template<typename NextApply>
auto apply_access_expire(const cache_builder_vars& b, NextApply&& next)
-> apply_access_expire_<std::decay_t<NextApply>> {
  return { b, std::forward<NextApply>(next) };
}


} /* namespace monsoon::cache::builder_detail::<unnamed> */


template<typename K, typename V, typename Impl, typename Hash, typename Eq, typename Create>
class wrapper final
: public cache_intf<K, V>,
  private Impl
{
 public:
  using store_type = typename Impl::store_type;

  template<typename Alloc, typename CreateArg>
  wrapper(const cache_builder<K, V, Hash, Eq, Alloc>& b,
      CreateArg&& create)
  : Impl(b),
    hash(b.hash()),
    eq(b.equality()),
    create(std::forward<CreateArg>(create))
  {}

  ~wrapper() noexcept {}

  auto get_if_present(const K& k) const
  -> std::shared_ptr<V> override {
    return this->lookup_if_present(
        make_cache_query(
            hash(k),
            [this, &k](const store_type& s) { return eq(s.key, k); },
            []() { assert(false); },
            []() { assert(false); }));
  }

  auto get(const K& k)
  -> std::shared_ptr<V> override {
    return this->lookup_or_create(
        make_cache_query(
            hash(k),
            [this, &k](const store_type& s) { return eq(s.key, k); },
            [this, &k]() { return std::make_tuple(k); },
            [this, &k](auto alloc) {
              return create(alloc, k);
            }));
  }

  Hash hash;
  Eq eq;
  Create create;
};


template<typename K, typename V, typename Impl, typename Hash, typename Eq, typename Alloc, typename Create>
class sharded_wrapper final
: public cache_intf<K, V>
{
 public:
  using store_type = typename Impl::store_type;
  static constexpr std::size_t hash_multiplier =
      (sizeof(std::size_t) <= 4
       ? 0x1001000fU // 257 * 1044751
       : 0x100010000001000fULL); // 3 * 3 * 18311 * 6996032116657

  sharded_wrapper(const sharded_wrapper&) = delete;
  sharded_wrapper(sharded_wrapper&&) = delete;
  sharded_wrapper& operator=(const sharded_wrapper&) = delete;
  sharded_wrapper& operator=(sharded_wrapper&&) = delete;

  template<typename CreateArg>
  sharded_wrapper(const cache_builder<K, V, Hash, Eq, Alloc>& b,
      unsigned int shards,
      CreateArg&& create)
  : hash(b.hash()),
    eq(b.equality()),
    create(std::forward<CreateArg>(create)),
    alloc_(b.allocator())
  {
    assert(shards > 1);
    using traits = typename std::allocator_traits<Alloc>::template rebind_traits<Impl>;

    traits::allocate(alloc_, shards);
    try {
      while (num_shards_ < shards) {
        traits::construct(alloc_, shards_ + num_shards_, b);
        ++num_shards_;
      }
    } catch (...) {
      while (num_shards_ > 0) {
        --num_shards_;
        traits::destroy(alloc_, shards_ + num_shards_);
      }
      traits::deallocate(alloc_, shards_, shards);
      throw;
    }
  }

  ~sharded_wrapper() noexcept {
    using traits = typename std::allocator_traits<Alloc>::template rebind_traits<Impl>;

    unsigned int shards = num_shards_;
    while (num_shards_ > 0) {
      --num_shards_;
      traits::destroy(alloc_, shards_ + num_shards_);
    }
    traits::deallocate(alloc_, shards_, shards);
  }

  auto get_if_present(const K& k) const
  -> std::shared_ptr<V> override {
    std::size_t hash_code = hash(k);

    return shards_[hash_multiplier * hash_code % num_shards_].lookup_if_present(
        make_cache_query(
            hash_code,
            [this, &k](const store_type& s) { return eq(s.key, k); },
            []() { assert(false); },
            []() { assert(false); }));
  }

  auto get(const K& k)
  -> std::shared_ptr<V> override {
    std::size_t hash_code = hash(k);
    return shards_[hash_multiplier * hash_code % num_shards_].lookup_or_create(
        make_cache_query(
            hash_code,
            [this, &k](const store_type& s) { return eq(s.key, k); },
            [this, &k]() { return std::make_tuple(k); },
            [this, &k](auto alloc) {
              return create(alloc, k);
            }));
  }

  Hash hash;
  Eq eq;
  Create create;

 private:
  typename std::allocator_traits<Alloc>::template rebind_alloc<Impl> alloc_;
  Impl* shards_ = nullptr;
  unsigned int num_shards_ = 0;
};


} /* namespace monsoon::cache::builder_detail */


template<typename K, typename V, typename Hash, typename Eq, typename Alloc>
template<typename Fn>
auto cache_builder<K, V, Hash, Eq, Alloc>::build(Fn&& fn) const
-> cache<K, V> {
  using namespace builder_detail;

  const unsigned int shards = (!thread_safe()
      ? 1u
      : (concurrency() == 0u ? std::max(1u, std::thread::hardware_concurrency()) : concurrency()));

  auto builder_impl =
      apply_access_expire(*this,
          apply_key_type(*this,
              apply_thread_safe(*this,
                  [this, &fn, shards](auto decorators) -> std::shared_ptr<cache_intf<K, V>> {
                    using basic_type = typename decltype(decorators)::template cache_type<V, Alloc>;
                    using wrapper_type = wrapper<K, V, basic_type, Hash, Eq, Fn>;
                    using sharded_wrapper_type = sharded_wrapper<K, V, basic_type, Hash, Eq, Alloc, Fn>;

                    if (shards != 1u) {
                      return std::allocate_shared<sharded_wrapper_type>(
                          allocator(),
                          *this, shards, std::forward<Fn>(fn));
                    } else {
                      return std::allocate_shared<wrapper_type>(
                          allocator(),
                          *this, std::forward<Fn>(fn));
                    }
                  })));

  return cache<K, V>(builder_impl(cache_decorator_set<>()));
}


} /* namespace monsoon::cache */

#endif /* MONSOON_CACHE_BUILDER_BUILD_H */
