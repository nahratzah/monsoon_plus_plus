#ifndef MONSOON_CACHE_BUILDER_BUILD_H
#define MONSOON_CACHE_BUILDER_BUILD_H

#include <type_traits>
#include <utility>
#include <memory>
#include <monsoon/cache/builder.h>
#include <monsoon/cache/simple_cache_impl.h>
#include <monsoon/cache/thread_safe_decorator.h>

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
              return std::allocate_shared<V>(alloc, create(k));
            }));
  }

  Hash hash;
  Eq eq;
  Create create;
};


} /* namespace monsoon::cache::builder_detail */


template<typename K, typename V, typename Hash, typename Eq, typename Alloc>
template<typename Fn>
auto cache_builder<K, V, Hash, Eq, Alloc>::build(Fn&& fn) const
-> cache<K, V> {
  using namespace builder_detail;

  auto builder_impl =
      apply_key_type(*this,
          apply_thread_safe(*this,
              [this, &fn](auto decorators) -> std::shared_ptr<cache_intf<K, V>> {
                using basic_type = typename decltype(decorators)::template cache_type<V, Alloc>;
                using wrapper_type = wrapper<K, V, basic_type, Hash, Eq, Fn>;
                return std::allocate_shared<wrapper_type>(
                    allocator(),
                    *this, std::forward<Fn>(fn));
              }));

  return cache<K, V>(builder_impl(cache_decorator_set<>()));
}


} /* namespace monsoon::cache */

#endif /* MONSOON_CACHE_BUILDER_BUILD_H */
