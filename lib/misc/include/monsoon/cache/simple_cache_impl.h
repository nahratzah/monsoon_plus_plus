#ifndef MONSOON_CACHE_SIMPLE_CACHE_IMPL_H
#define MONSOON_CACHE_SIMPLE_CACHE_IMPL_H

#include <monsoon/cache/bucket.h>
#include <monsoon/cache/cache.h>
#include <monsoon/cache/create_handler.h>
#include <monsoon/cache/key_decorator.h>
#include <monsoon/cache/cache_query.h>
#include <cmath>
#include <stdexcept>

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


template<typename Bucket, typename... CacheDecorators>
struct decorate_bucket_ {
  using type = typename bucket_add_nonvoid_types_<Bucket, typename decorator_for_<CacheDecorators>::type...>::type;
};

template<typename Elem, typename... CacheDecorators>
using decorate_bucket_t = typename decorate_bucket_<Elem, CacheDecorators...>::type;


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
struct decorators_on_create_<S> {
  template<typename T>
  static auto apply(S& s, T& v) {}
};

template<typename S, typename D0, typename... D>
struct decorators_on_create_<S, D0, D...> {
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
struct decorators_on_delete_<S> {
  template<typename T>
  static auto apply(S& s, T& v) {}
};

template<typename S, typename D0, typename... D>
struct decorators_on_delete_<S, D0, D...> {
  template<typename T>
  static auto apply(S& s, T& v) {
    decorator_on_delete_<S, D0>::apply(s, v);
    decorators_on_delete_<S, D...>::apply(s, v);
  }
};


template<typename S, typename D, typename = void>
struct decorator_on_hit_ {
  static auto apply(S& s, D& d) -> void {}
};

template<typename S, typename D>
struct decorator_on_hit_<S, D, std::void_t<decltype(std::declval<D&>().on_hit(std::declval<S&>()))>> {
  static_assert(noexcept(std::declval<D&>().on_hit(std::declval<S&>())),
      "on_hit must be a noexcept function.");
  static auto apply(S& s, D& d) -> void { d.on_hit(s); }
};

template<typename S, typename... D>
struct decorators_on_hit_;

template<typename S>
struct decorators_on_hit_<S> {
  template<typename T>
  static auto apply(S& s, T& v) noexcept {}
};

template<typename S, typename D0, typename... D>
struct decorators_on_hit_<S, D0, D...> {
  template<typename T>
  static auto apply(S& s, T& v) noexcept -> void {
    decorator_on_hit_<S, D0>::apply(s, v);
    decorators_on_hit_<S, D...>::apply(s, v);
  }
};


template<typename D, typename = void>
struct cache_decorator_tpl_ {
  static auto apply(D& d) -> decltype(auto) {
    return std::tuple<>();
  }
};

template<typename D>
struct cache_decorator_tpl_<D, std::void_t<decltype(std::declval<D&>().init_tuple())>> {
  static auto apply(D& d) -> decltype(auto) {
    return d.init_tuple();
  }
};


} /* namespace monsoon::cache::<unnamed> */

template<typename T, typename Alloc, typename... CacheDecorators>
class simple_cache_impl
: public CacheDecorators...
{
 public:
  using size_type = std::uintptr_t;

 private:
  using bucket_type = decorate_bucket_t<bucket<T>, CacheDecorators...>;
  using bucket_vector = std::vector<bucket_type, typename std::allocator_traits<Alloc>::template rebind_alloc<bucket_type>>;
  using lookup_type = typename bucket_type::lookup_type;

 public:
  using alloc_t = Alloc; // Not allocator_type, to prevent accidents with std::uses_allocator.
  using store_type = typename bucket_type::store_type;
  using pointer = typename store_type::pointer;

 private:
  ///\brief Initial number of buckets.
  ///\note Must be at least 1.
  static constexpr unsigned int init_bucket_count = 3;
  ///\brief Divider in growth ratio.
  static constexpr unsigned int growth_div = 7;
  ///\brief Numerator in growth ratio.
  static constexpr unsigned int growth_mul = 9;

 public:
  template<typename Key, typename Hash, typename Eq>
  simple_cache_impl(const cache_builder<Key, T, Hash, Eq, Alloc>& b)
  : CacheDecorators(b)...,
    buckets_(b.allocator()),
    alloc_(b.allocator()),
    lf_(b.load_factor())
  {
    buckets_.resize(init_bucket_count); // May not be zero, or we'll get (unchecked) division by zero.
  }

  ~simple_cache_impl() noexcept;

  auto load_factor() const noexcept -> float;
  auto max_load_factor() const noexcept -> float;
  auto max_load_factor(float fl) -> void;
  auto size() const noexcept -> size_type;

  template<typename Predicate, typename TplBuilder, typename Create>
  auto lookup_if_present(const cache_query<Predicate, TplBuilder, Create>& q) noexcept -> pointer {
    // Acquire lock on the cache.
    // One of the decorators is to supply lock() and unlock() methods,
    // that can be called on a const-reference of this.
    std::unique_lock<const simple_cache_impl> lck{ *this };

    // Prepare query.
    const auto query = make_bucket_ctx(
        q.hash_code,
        q.predicate,
        []() -> store_type { throw std::runtime_error("create should not be called"); },
        size_,
        [this](store_type& s) { decorators_on_hit_<store_type, CacheDecorators...>::apply(s, *this); },
        [this](store_type& s) { this->on_delete(s); });

    // Execute query.
    assert(buckets_.size() > 0);
    return resolve_(lck, buckets_[query.hash_code % buckets_.size()]
        .lookup_if_present(query));
  }

  template<typename Predicate, typename TplBuilder, typename Create>
  auto lookup_or_create(const cache_query<Predicate, TplBuilder, Create>& q) noexcept -> pointer {
    // Acquire lock on the cache.
    // One of the decorators is to supply lock() and unlock() methods,
    // that can be called on a const-reference of this.
    std::unique_lock<const simple_cache_impl> lck{ *this };
    auto ch = make_create_handler<store_type::is_async>(q.create);

    // Prepare query.
    const auto query = make_bucket_ctx(
        q.hash_code,
        q.predicate,
        [this, &q, &ch]() -> store_type {
          return store_type(
              std::allocator_arg, alloc_,
              ch(alloc_), q.hash_code,
              std::tuple_cat(q.tpl_builder(), cache_decorator_tpl_<CacheDecorators>::apply(*this)...));
        },
        size_,
        [this](store_type& s) { decorators_on_hit_<store_type, CacheDecorators...>::apply(s, *this); },
        [this](store_type& s) { this->on_delete(s); });

    // Execute query.
    store_type* created;
    assert(buckets_.size() > 0);
    lookup_type lookup_result = buckets_[query.hash_code % buckets_.size()]
        .lookup_or_create(alloc_, query, created);
    assert(!store_type::is_nil(lookup_result));
    pointer result = resolve_(lck, std::move(lookup_result), created);
    assert(result != nullptr);

    // If newly created, call post processing hooks.
    if (created != nullptr) {
      if (!lck.owns_lock()) lck.lock(); // Relock.
      maybe_rehash_();
      on_create(*created);
    }

    return result;
  }

 private:
  /**
   * \brief Handle resolution of shared future component of lookup type.
   * \details This function is a noop if the lookup_type doesn't have a shared future component.
   *
   * If the lookup type was based on a shared future,
   * \ref element::resolve() "created->resolve()" will be called to ensure
   * the \ref element "store type" is updated properly.
   * \note After this call, it is undefined wether lck may will be in a locked state, or unlocked state.
   * \param[in] lck The unique lock locking this.
   * \param[in] l The result of a lookup operation.
   * \param[created] A pointer to the created store_type. Nullptr if no store type was created.
   * \returns Pointer from the lookup type.
   */
  static auto resolve_(std::unique_lock<const simple_cache_impl>& lck,
      lookup_type&& l,
      store_type* created = nullptr)
  -> pointer;

  ///\brief Perform rehashing, if load factor constraint requires it.
  void maybe_rehash_() noexcept;

  ///\brief Invoke on_create method for each cache decorator that implements it.
  void on_create(store_type& s) noexcept;
  ///\brief Invoke on_delete method for each cache decorator that implements it.
  void on_delete(store_type& s) noexcept;

  bucket_vector buckets_;
  Alloc alloc_;
  float lf_ = 1.0; ///<\brief Target load factor.
  size_type size_ = 0;
};


template<typename T, typename Alloc, typename... CacheDecorators>
simple_cache_impl<T, Alloc, CacheDecorators...>::~simple_cache_impl() noexcept {
  for (auto& b : buckets_)
    b.erase_all(alloc_, size_, [this](store_type& s) { on_delete(s); });
}

template<typename T, typename A, typename... D>
auto simple_cache_impl<T, A, D...>::load_factor() const
noexcept
-> float {
  return std::double_t(size_) / std::double_t(buckets_.size());
}

template<typename T, typename A, typename... D>
auto simple_cache_impl<T, A, D...>::max_load_factor() const
noexcept
-> float {
  return lf_;
}

template<typename T, typename A, typename... D>
auto simple_cache_impl<T, A, D...>::max_load_factor(float lf)
-> void {
  if (lf <= 0.0 || !std::isfinite(lf))
    throw std::invalid_argument("invalid load factor");
  lf_ = lf;
  maybe_rehash_();
}

template<typename T, typename A, typename... D>
auto simple_cache_impl<T, A, D...>::size() const noexcept
-> size_type {
  return size_;
}

template<typename T, typename A, typename... D>
auto simple_cache_impl<T, A, D...>::resolve_(
    std::unique_lock<const simple_cache_impl>& lck,
    lookup_type&& l,
    store_type* created)
-> pointer {
  if constexpr(std::is_same_v<pointer, lookup_type>) {
    return std::move(l);
  } else {
    return std::visit(
        overload(
            [](pointer p) -> pointer { return std::move(p); },
            [&lck, created](std::shared_future<pointer>&& fut) -> pointer {
              lck.unlock(); // Don't hold the lock during future resolution!
              if (created == nullptr)
                return fut.get();

              // Resolve future with the cache unlocked.
              // This allows:
              // - other threads to access the cache without blocking on
              //   our create function,
              // - the create function to recurse into the cache,
              // - the allocators involved in the create function,
              //   to perform cache maintenance routines.
              fut.wait();

              // Relock before calling resolve,
              // because element mutation is not thread safe.
              lck.lock();

              return created->resolve(); // Installs result of future.
            }),
        std::move(l));
  }
}

template<typename T, typename A, typename... D>
auto simple_cache_impl<T, A, D...>::maybe_rehash_()
noexcept // Allocation exception is swallowed.
-> void {
  // Check if we require rehashing.
  const std::double_t target_buckets_dbl = std::ceil(std::double_t(size_) / lf_);
  typename bucket_vector::size_type target_buckets = buckets_.max_size();
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
      target_buckets = std::min(b_mul, buckets_.max_size());
    }
    if (buckets_.size() >= target_buckets)
      return; // No rehash required.
  }

  // Perform rehashing operation.
  const typename bucket_vector::size_type orig_size = buckets_.size();
  try {
    buckets_.resize(target_buckets);
  } catch (...) {
    // Swallow exception.
    return;
  }

  {
    typename bucket_vector::size_type b_idx = 0;
    for (auto i = buckets_.begin(), i_end = buckets_.begin() + orig_size;
        i != i_end;
        ++i, ++b_idx) {
      i->rehash(
          [this](const std::size_t hash_code) -> bucket_type& {
            return buckets_[hash_code % buckets_.size()];
          });
    }
  }
}

template<typename T, typename A, typename... D>
void simple_cache_impl<T, A, D...>::on_create(store_type& s) noexcept {
  decorators_on_create_<store_type, D...>::apply(s, *this);
}

template<typename T, typename A, typename... D>
void simple_cache_impl<T, A, D...>::on_delete(store_type& s) noexcept {
  decorators_on_delete_<store_type, D...>::apply(s, *this);
}


} /* namespace monsoon::cache */

#endif /* MONSOON_CACHE_SIMPLE_CACHE_IMPL_H */
