#ifndef MONSOON_CACHE_BUILDER_H
#define MONSOON_CACHE_BUILDER_H

///\file
///\ingroup cache

#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>

namespace monsoon::cache {


template<typename T, typename U, typename Hash, typename Eq, typename Alloc, typename Create>
class extended_cache;


///\brief Type agnostic data in \ref cache_builder "cache builder".
class cache_builder_vars {
 public:
  ///\bug Not implemented.
  auto max_memory() const noexcept -> std::optional<std::uintptr_t>;
  ///\bug Not implemented.
  auto max_size() const noexcept -> std::optional<std::uintptr_t>;
  auto max_age() const noexcept -> std::optional<std::chrono::seconds>;
  auto access_expire() const noexcept -> std::optional<std::chrono::seconds>;
  auto thread_safe() const noexcept -> bool;
  auto concurrency() const noexcept -> unsigned int;
  auto load_factor() const noexcept -> float;

 protected:
  std::optional<std::uintptr_t> max_memory_, max_size_;
  std::optional<std::chrono::seconds> max_age_, access_expire_;
  bool thread_safe_ = true;
  unsigned int concurrency_ = 0; // zero signifies to use std::hardware_concurrency
  float lf_ = 1.0;
};

/**
 * \brief Cache builder.
 * \details Contains all parameters to build a cache.
 * \tparam T The key type of the cache.
 * \tparam U The mapped type of the cache.
 * \tparam Hash Hash function on the key type.
 * \tparam Eq Equality predicate on the key type.
 * \tparam Alloc Allocator to use for cache elements.
 */
template<typename T, typename U,
    typename Hash = std::hash<T>,
    typename Eq = std::equal_to<T>,
    typename Alloc = std::allocator<U>>
class cache_builder
: public cache_builder_vars
{
 public:
  using key_type = T;
  using mapped_type = U;

  constexpr explicit cache_builder(
      Hash hash = Hash(), Eq eq = Eq(), Alloc alloc = Alloc());

  template<typename OtherT, typename OtherU, typename OtherHash, typename OtherEq, typename OtherAlloc>
  constexpr explicit cache_builder(
      const cache_builder_vars& other,
      Hash hash = Hash(), Eq eq = Eq(), Alloc alloc = Alloc());

  using cache_builder_vars::max_memory;
  constexpr auto max_memory(std::uintptr_t v) noexcept -> cache_builder&;
  constexpr auto no_max_memory() noexcept -> cache_builder&;

  using cache_builder_vars::max_size;
  constexpr auto max_size(std::uintptr_t v) noexcept -> cache_builder&;
  constexpr auto no_max_size() noexcept -> cache_builder&;

  using cache_builder_vars::max_age;
  constexpr auto max_age(std::chrono::seconds d) -> cache_builder&;
  constexpr auto no_max_age() noexcept -> cache_builder&;

  using cache_builder_vars::access_expire;
  constexpr auto access_expire(std::chrono::seconds d) -> cache_builder&;
  constexpr auto no_access_expire() noexcept -> cache_builder&;

  constexpr auto no_expire() noexcept -> cache_builder&;

  using cache_builder_vars::thread_safe;
  constexpr auto thread_safe() noexcept -> cache_builder&;
  constexpr auto not_thread_safe() noexcept -> cache_builder&;

  using cache_builder_vars::concurrency;
  constexpr auto with_concurrency(unsigned int n = 0) noexcept -> cache_builder&;
  constexpr auto no_concurrency() noexcept -> cache_builder&;

  using cache_builder_vars::load_factor;
  auto load_factor(float lf) -> cache_builder&;

  template<typename NewHash>
  auto with_hash(NewHash hash) const
  -> cache_builder<T, U, NewHash, Eq, Alloc>;

  template<typename NewEq>
  auto with_equality(NewEq eq) const
  -> cache_builder<T, U, Hash, NewEq, Alloc>;

  template<typename NewAlloc>
  auto with_allocator(NewAlloc alloc) const
  -> cache_builder<T, U, Hash, Eq, NewAlloc>;

  /**
   * \brief Build the cache described by this builder.
   * \note You must include <monsoon/cache/impl.h> if you call this function,
   * since that header contains the implementation.
   * \returns A new cache described by the specification in this.
   */
  template<typename Fn>
  auto build(Fn&& fn) const
  -> extended_cache<T, U, Hash, Eq, Alloc, std::decay_t<Fn>>;

  auto hash() const noexcept -> Hash;
  auto equality() const noexcept -> Eq;
  auto allocator() const noexcept -> Alloc;

 private:
  Hash hash_;
  Eq eq_;
  Alloc alloc_;
};

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
constexpr cache_builder<T, U, Hash, Eq, Alloc>::cache_builder(Hash hash, Eq eq, Alloc alloc)
: hash_(std::move(hash)),
  eq_(std::move(eq)),
  alloc_(std::move(alloc))
{}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
template<typename OtherT, typename OtherU, typename OtherHash, typename OtherEq, typename OtherAlloc>
constexpr cache_builder<T, U, Hash, Eq, Alloc>::cache_builder(
    const cache_builder_vars& other,
    Hash hash, Eq eq, Alloc alloc)
: cache_builder_vars(other),
  hash_(std::move(hash_)),
  eq_(std::move(eq_)),
  alloc_(std::move(alloc))
{}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
constexpr auto cache_builder<T, U, Hash, Eq, Alloc>::max_memory(std::uintptr_t v)
noexcept
-> cache_builder& {
  max_memory_ = v;
  return *this;
}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
constexpr auto cache_builder<T, U, Hash, Eq, Alloc>::no_max_memory()
noexcept
-> cache_builder& {
  max_memory_.reset();
  return *this;
}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
constexpr auto cache_builder<T, U, Hash, Eq, Alloc>::max_size(std::uintptr_t v)
noexcept
-> cache_builder& {
  max_size_ = v;
  return *this;
}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
constexpr auto cache_builder<T, U, Hash, Eq, Alloc>::no_max_size()
noexcept
-> cache_builder& {
  max_size_.reset();
  return *this;
}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
constexpr auto cache_builder<T, U, Hash, Eq, Alloc>::max_age(std::chrono::seconds d)
-> cache_builder& {
  if (d <= std::chrono::seconds::zero())
    throw std::invalid_argument("negative/zero expiry");
  max_age_ = d;
  return *this;
}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
constexpr auto cache_builder<T, U, Hash, Eq, Alloc>::no_max_age()
noexcept
-> cache_builder& {
  max_age_.reset();
  return *this;
}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
constexpr auto cache_builder<T, U, Hash, Eq, Alloc>::access_expire(std::chrono::seconds d)
-> cache_builder& {
  if (d <= std::chrono::seconds::zero())
    throw std::invalid_argument("negative/zero expiry");
  access_expire_ = d;
  return *this;
}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
constexpr auto cache_builder<T, U, Hash, Eq, Alloc>::no_access_expire()
noexcept
-> cache_builder& {
  access_expire_.reset();
  return *this;
}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
constexpr auto cache_builder<T, U, Hash, Eq, Alloc>::no_expire()
noexcept
-> cache_builder& {
  return (*this)
      .no_max_age()
      .no_access_expire();
}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
constexpr auto cache_builder<T, U, Hash, Eq, Alloc>::thread_safe()
noexcept
-> cache_builder& {
  thread_safe_ = true;
  return *this;
}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
constexpr auto cache_builder<T, U, Hash, Eq, Alloc>::not_thread_safe()
noexcept
-> cache_builder& {
  thread_safe_ = false;
  return *this;
}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
constexpr auto cache_builder<T, U, Hash, Eq, Alloc>::with_concurrency(unsigned int n)
noexcept
-> cache_builder& {
  concurrency_ = n;
  return this->thread_safe();
}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
constexpr auto cache_builder<T, U, Hash, Eq, Alloc>::no_concurrency()
noexcept
-> cache_builder& {
  this->concurrency_ = 1;
  return *this;
}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
auto cache_builder<T, U, Hash, Eq, Alloc>::load_factor(float lf)
-> cache_builder& {
  if (lf <= 0.0 || !std::isfinite(lf))
    throw std::invalid_argument("invalid load factor");
  lf_ = lf;
  return *this;
}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
template<typename NewHash>
auto cache_builder<T, U, Hash, Eq, Alloc>::with_hash(NewHash hash) const
-> cache_builder<T, U, NewHash, Eq, Alloc> {
  return cache_builder<T, U, NewHash, Eq, Alloc>(*this, hash, eq_, alloc_);
}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
template<typename NewEq>
auto cache_builder<T, U, Hash, Eq, Alloc>::with_equality(NewEq eq) const
-> cache_builder<T, U, Hash, NewEq, Alloc> {
  return cache_builder<T, U, Hash, NewEq, Alloc>(*this, hash_, eq, alloc_);
}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
template<typename NewAlloc>
auto cache_builder<T, U, Hash, Eq, Alloc>::with_allocator(NewAlloc alloc) const
-> cache_builder<T, U, Hash, Eq, NewAlloc> {
  return cache_builder<T, U, Hash, Eq, NewAlloc>(*this, hash_, eq_, alloc);
}

inline auto cache_builder_vars::max_memory() const
noexcept
-> std::optional<std::uintptr_t> {
  return max_memory_;
}

inline auto cache_builder_vars::max_size() const
noexcept
-> std::optional<std::uintptr_t> {
  return max_size_;
}

inline auto cache_builder_vars::max_age() const
noexcept
-> std::optional<std::chrono::seconds> {
  return max_age_;
}

inline auto cache_builder_vars::access_expire() const
noexcept
-> std::optional<std::chrono::seconds> {
  return access_expire_;
}

inline auto cache_builder_vars::thread_safe() const
noexcept
-> bool {
  return thread_safe_;
}

inline auto cache_builder_vars::concurrency() const
noexcept
-> unsigned int {
  return concurrency_;
}

inline auto cache_builder_vars::load_factor() const
noexcept
-> float {
  return lf_;
}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
auto cache_builder<T, U, Hash, Eq, Alloc>::hash() const
noexcept
-> Hash {
  return hash_;
}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
auto cache_builder<T, U, Hash, Eq, Alloc>::equality() const
noexcept
-> Eq {
  return eq_;
}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
auto cache_builder<T, U, Hash, Eq, Alloc>::allocator() const
noexcept
-> Alloc {
  return alloc_;
}


} /* namespace monsoon::cache */

#endif /* MONSOON_CACHE_BUILDER_H */
