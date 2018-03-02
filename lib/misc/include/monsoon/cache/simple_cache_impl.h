#ifndef MONSOON_CACHE_SIMPLE_CACHE_IMPL_H
#define MONSOON_CACHE_SIMPLE_CACHE_IMPL_H

#include <monsoon/cache/bucket.h>
#include <monsoon/cache/cache.h>
#include <monsoon/cache/key_decorator.h>
#include <cmath>
#include <scoped_allocator>

namespace monsoon::cache {
namespace {


template<typename CacheDecorator, typename = void>
struct decorator_for_ {
  using type = void;
};

template<typename CacheDecorator>
struct decorator_for_<CacheDecorator, std::void_t<typename CacheDecorator::element_decorator_type>> {
  using type = typename CacheDecorator::element_decorator_type;
};


template<typename Elem, typename... Args> struct bucket_add_nonvoid_types_;

template<typename... ElemArgs, typename Arg0, typename... Args>
struct bucket_add_nonvoid_types_<bucket<ElemArgs...>, Arg0, Args...> {
  using type = typename bucket_add_nonvoid_types_<bucket<ElemArgs..., Arg0>, Args...>::type;
};

template<typename... ElemArgs, typename... Args>
struct bucket_add_nonvoid_types_<bucket<ElemArgs...>, void, Args...> {
  using type = typename bucket_add_nonvoid_types_<bucket<ElemArgs...>, Args...>::type;
};

template<typename... ElemArgs>
struct bucket_add_nonvoid_types_<bucket<ElemArgs...>> {
  using type = bucket<ElemArgs...>;
};


template<typename Bucket, typename... CacheDecorators> struct decorate_bucket_;

template<typename Bucket, typename CacheDecorators>
struct decorate_bucket_<Bucket, CacheDecorators> {
  using type = typename bucket_add_nonvoid_types_<Bucket, decorate_for_<CacheDecorators>...>::type;
};


template<typename S, typename D, typename = void>
struct decorator_on_create_ {
  static auto apply(S& s, D& d) {}
};

template<typename S, typename D>
struct decorator_on_create_<S, D, std::void_t<decltype(std::declval<D&>().on_create(std::declval<S&>()))>> {
  static_assert(noexcept(std::declval<D&>().on_create(std::declval<S&>())),
      "on_create must be a noexcept function.");
  static auto apply(S& s, D& d) { d.on_create(s); }
};

template<typename S, typename... D>
struct decorators_on_create_;

template<typename S>
struct decorators_on_create_ {
  template<typename T>
  static auto apply(S& s, T& v) {}
};

template<typename S, typename D0, typename... D>
struct decorators_on_create_ {
  template<typename T>
  static auto apply(S& s, T& v) {
    decorator_on_create_<S, D0>::apply(s, v);
    decorators_on_create_<S, D...>::apply(s, v);
  }
};


template<typename S, typename D, typename = void>
struct decorator_on_delete_ {
  static auto apply(S& s, D& d) {}
};

template<typename S, typename D>
struct decorator_on_delete_<S, D, std::void_t<decltype(std::declval<D&>().on_delete(std::declval<S&>()))>> {
  static_assert(noexcept(std::declval<D&>().on_delete(std::declval<S&>())),
      "on_create must be a noexcept function.");
  static auto apply(S& s, D& d) { d.on_delete(s); }
};

template<typename S, typename... D>
struct decorators_on_delete_;

template<typename S>
struct decorators_on_delete_ {
  template<typename T>
  static auto apply(S& s, T& v) {}
};

template<typename S, typename D0, typename... D>
struct decorators_on_delete_ {
  template<typename T>
  static auto apply(S& s, T& v) {
    decorator_on_delete_<S, D0>::apply(s, v);
    decorators_on_delete_<S, D...>::apply(s, v);
  }
};


} /* namespace monsoon::cache::<unnamed> */

template<typename Elem, typename... CacheDecorators>
using decorate_bucket_t = typename decorate_bucket_<Elem, CacheDecorators...>::type;

template<typename Predicate, typename TplBuilder, typename Create>
class cache_query {
  cache_query(std::size_t hash_code, Predicate predicate, TplBuilder tpl_builder, Create create)
  noexcept(std::is_nothrow_move_constructible_v<Predicate>()
      && std::is_nothrow_move_constructible_v<TplBuilder>()
      && std::is_nothrow_move_constructible_v<Create>())
  : hash_code(hash_code),
    eq(std::move(eq)),
    tpl_builder(std::move(tpl_builder)),
    create(std::move(create))
  {}

  std::size_t hash_code;
  Predicate predicate;
  TplBuilder tpl_builder;
  Create create;
};

template<typename Predicate, typename TplBuilder, typename Create>
auto make_cache_query(std::size_t hash_code, Predicate&& predicate, TplBuilder&& tpl_builder, Create&& create)
-> cache_query<Predicate, TplBuilder, Create> {
  return {
    hash_code,
    std::forward<Predicate>(predicate),
    std::forward<TplBuilder>(tpl_builder),
    std::forward<Create>(create)
  };
}

template<typename T, typename Alloc, typename... CacheDecorators>
class simple_cache_impl
: public CacheDecorators...
{
 public:
  using key_type = Key;
  using bucket_type = decorate_bucket_t<bucket<T, Alloc>, CacheDecorators...>;
  using size_type = std::uintptr_t;

 private:
  using bucket_vector = std::vector<bucket_type, typename std::allocator_traits<Alloc>::template rebind<bucket_type>>;

 public:
  using store_type = typename bucket_type::store_type;
  using pointer = typename store_type::pointer;
  using lookup_type = typename bucket_type::lookup_type;

 private:
  ///\brief Initial number of buckets.
  ///\note Must be at least 1.
  static constexpr unsigned int init_bucket_count = 3;
  ///\brief Divider in growth ratio.
  static constexpr unsigned int growth_div = 7;
  ///\brief Numerator in growth ratio.
  static constexpr unsigned int growth_mul = 9;

 public:
  simple_cache_impl(cache_builder<Key, Val, Hash, Eq, Alloc> b, Create create)
  : CacheDecorators(b)...,
    buckets_(b.get_allocator()),
    hash_(b.hash()),
    eq_(b.equality()),
    create_(std::move(create)),
    alloc_(b.get_allocator()),
    lf_(b.load_factor())
  {
    buckets_.resize(init_bucket_count);
  }

  auto load_factor() const noexcept -> float;
  auto max_load_factor() const noexcept -> float;
  auto max_load_factor(float fl) -> void;
  auto size() const noexcept;

  template<typename Predicate, typename TplBuilder>
  auto lookup_if_present(const cache_query<Predicate, TplBuilder>& q) const noexcept -> lookup_type {
    // Prepare query.
    const auto query = make_bucket_ctx(
        q.hash_code,
        q.predicate,
        []() -> store_type { throw std::runtime_exception("create should not be called"); },
        size_);

    // Execute query.
    return buckets_[query.hash_code % buckets_.size()]
        .lookup_if_present(query);
  }

  template<typename Predicate, typename TplBuilder>
  auto lookup_or_create(const cache_query<Predicate, TplBuilder>& q) const noexcept -> lookup_type {
    // Prepare query.
    const auto query = make_bucket_ctx(
        q.hash_code,
        q.predicate,
        [this, &q]() -> store_type {
          return store_type(
              std::allocator_arg, alloc_,
              q.create(alloc_), q.hash_code,
              std::forward_as_tuple(q.tpl_builder()));
        },
        size_);

    // Execute query.
    store_type* created;
    auto result = buckets_[hash_code % buckets_.size()]
        .lookup_or_create(query, created);
    assert(result != nullptr);

    // If newly created, call post processing hooks.
    if (created != nullptr) {
      maybe_rehash_();
      on_create(*created);
    }

    return result;
  }

 private:
  ///\brief Perform rehashing, if load factor constraint requires it.
  void maybe_rehash_() noexcept;

  ///\brief Invoke on_create method for each cache decorator that implements it.
  void on_create(store_type& s) noexcept {
    decorators_on_create_<store_type, CacheDecorators...>::apply(s, *this);
  }

  ///\brief Invoke on_delete method for each cache decorator that implements it.
  void on_create(store_type& s) noexcept {
    decorators_on_delete_<store_type, CacheDecorators...>::apply(s, *this);
  }

  bucket_vector buckets_;
  float lf_ = 1.0; ///<\brief Target load factor.
  Alloc alloc_;
  size_type size_ = 0;
};


template<typename Key, typename Val, typename Hash, typename Eq, typename Create, typename Alloc>
auto simple_cache_impl<Key, Val, Hash, Eq, Create, Alloc>::load_factor() const
noexcept
-> float {
  return std::double_t(size_) / std::double_t(buckets_.size());
}

template<typename Key, typename Val, typename Hash, typename Eq, typename Create, typename Alloc>
auto simple_cache_impl<Key, Val, Hash, Eq, Create, Alloc>::load_factor() const
noexcept
-> float {
  return lf_;
}

template<typename Key, typename Val, typename Hash, typename Eq, typename Create, typename Alloc>
auto simple_cache_impl<Key, Val, Hash, Eq, Create, Alloc>::load_factor(float lf)
-> float {
  if (lf <= 0.0 || !std::isfinite(lf))
    throw std::invalid_argument("invalid load factor");
  lf_ = lf;
  maybe_rehash_();
}

template<typename Key, typename Val, typename Hash, typename Eq, typename Create, typename Alloc>
auto simple_cache_impl<Key, Val, Hash, Eq, Create, Alloc>::size() const
-> size_type {
  return size_;
}

template<typename Key, typename Val, typename Hash, typename Eq, typename Create, typename Alloc>
auto simple_cache_impl<Key, Val, Hash, Eq, Create, Alloc>::maybe_rehash_()
noexcept // Allocation exception is swallowed.
-> void {
  // Check if we require rehashing.
  const std::double_t target_buckets_dbl = std::ceil(std::double_t(size_) / lf_);
  bucket_vector::size_type target_buckets = buckets_.max_size();
  if (target_buckets_dbl < target_buckets)
    target_buckets = target_buckets_dbl;
  if (buckets_.size() >= target_buckets) return; // No rehash required.

  // Compute new number of buckets.
  {
    const auto b_div = buckets_.size() / growth_div + 1;
    const auto b_mul = b_div * growth_mul;
    if (b_mul / growth_mul != b_div) { // Overflow.
      target_buckets = buckets_.max_size();
    } else {
      target_buckets = std::min(b_mul, buckets_max.size());
    }
    if (buckets_.size() >= target_buckets)
      return; // No rehash required.
  }

  // Perform rehashing operation.
  orig_size = buckets_.size();
  try {
    buckets_.resize(target_buckets);
  } catch (...) {
    // Swallow exception.
    return;
  }

  {
    b_idx = 0;
    for (i = buckets_.begin(), i_end = buckets_.begin() + orig_size;
        i != i_end;
        ++i, ++b_idx) {
      i->rehash(
          [this](const std::size_t hash_code) -> bucket_type& {
            return buckets_[hash_code % buckets_.size()];
          });
    }
  }
}


} /* namespace monsoon::cache */

#endif /* MONSOON_CACHE_SIMPLE_CACHE_IMPL_H */
