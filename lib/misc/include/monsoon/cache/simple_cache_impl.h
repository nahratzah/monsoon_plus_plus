#ifndef MONSOON_CACHE_SIMPLE_CACHE_IMPL_H
#define MONSOON_CACHE_SIMPLE_CACHE_IMPL_H

#include <monsoon/cache/bucket.h>
#include <monsoon/cache/key_decorator.h>
#include <cmath>
#include <scoped_allocator>

namespace monsoon::cache {


template<typename Key, typename Val, typename Hash, typename Eq, typename Create, typename Alloc>
class simple_cache_impl {
 public:
  using key_type = Key;
  using bucket_type = bucket<Val, Alloc, key_decorator<key_type>>;

 private:
  using bucket_vector = std::vector<bucket, std::scoped_allocator_adaptor<Alloc, Alloc>>;

 public:
  using store_type = typename bucket_type::store_type;
  using pointer = typename store_type::pointer;

  simple_cache_impl(cache_builder<Key, Val, Hash, Eq, Alloc> b, Create create)
  : hash_(b.hash()),
    eq_(b.equality()),
    create_(std::move(create)),
    alloc_(b.get_allocator()),
    lf_(b.load_factor())
  {}

  auto load_factor() const noexcept -> float;
  auto max_load_factor() const noexcept -> float;
  auto max_load_factor(float fl) -> void;
  auto size() const noexcept;

  auto lookup_if_present(const key_type& k) const noexcept {
    // Prepare query.
    const auto query = make_bucket_ctx(
        hash_(k),
        [this, &k](const store_type& s) { return eq_(s.key, k); },
        []() -> store_type { throw std::runtime_exception("create should not be called"); });

    // Execute query.
    return buckets_[query.hash_code % buckets_.size()]
        .lookup_if_present(query);
  }

  auto lookup_or_create(const key_type& k) const noexcept {
    // Prepare query.
    std::size_t hash_code = hash_(k);
    const auto query = make_bucket_ctx(
        hash_code,
        [this, &k](const store_type& s) { return eq_(s.key, k); },
        [this, hash_code, &k, &created]() -> store_type {
          created = true;
          return store_type(std::allocate_shared<Val>(alloc_, create_(k)), hash_code, std::forward_as_tuple(k));
        });

    // Execute query.
    store_type* created;
    auto result = buckets_[hash_code % buckets_.size()]
        .lookup_or_create(query, created);
    assert(result != nullptr);

    // If newly created, weaken for automatic expiry and rehash.
    if (created != nullptr) {
      created->weaken();
      maybe_rehash_();
    }

    return result;
  }

 private:
  ///\brief Perform rehashing, if load factor constraint requires it.
  void maybe_rehash_() noexcept;

  bucket_vector buckets_;
  float lf_ = 1.0; ///<\brief Target load factor.
  Hash hash_;
  Eq eq_;
  Create create_;
  Alloc alloc_;
};


template<typename Key, typename Val, typename Hash, typename Eq, typename Create, typename Alloc>
auto simple_cache_impl<Key, Val, Hash, Eq, Create, Alloc>::load_factor() const
noexcept
-> float {
  return std::double_t(size()) / std::double_t(buckets_.size());
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
  size_type i = 0;
  for (const bucket_type& b : data_)
    i += b.size();
  return i;
}

template<typename Key, typename Val, typename Hash, typename Eq, typename Create, typename Alloc>
auto simple_cache_impl<Key, Val, Hash, Eq, Create, Alloc>::maybe_rehash_()
noexcept // Allocation exception is swallowed.
-> void {
  const std::double_t target_buckets_dbl = std::ceil(std::double_t(size()) / lf_);
  bucket_vector::size_type target_buckets = buckets_.max_size();
  if (target_buckets_dbl < target_buckets)
    target_buckets = target_buckets_dbl;
  if (buckets_.size() >= target_buckets) return; // No rehash required.

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
