#ifndef MONSOON_CACHE_BUILDER_H
#define MONSOON_CACHE_BUILDER_H

#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>

namespace monsoon::cache {


template<typename T, typename U,
    typename Hash = std::hash<T>,
    typename Eq = std::equal_to<T>,
    typename Alloc = std::allocator<U>>
class cache_builder {
 public:
  constexpr explicit cache_builder(
      Hash hash = Hash(), Eq eq = Eq(), Alloc alloc = Alloc());
  constexpr explicit cache_builder(
      const cache_builder<OtherT, OtherU, OtherHash, OtherEq, OtherAlloc>& other,
      Hash hash = Hash(), Eq eq = Eq(), Alloc alloc = Alloc());

  constexpr auto max_memory(std::uintptr_t v) noexcept -> cache_builder&;
  constexpr auto no_max_memory() noexcept -> cache_builder&;

  constexpr auto max_size(std::uintptr_t v) noexcept -> cache_builder&;
  constexpr auto no_max_size() noexcept -> cache_builder&;

  constexpr auto max_age(std::chrono::seconds d) -> cache_builder&;
  constexpr auto no_max_age() noexcept -> cache_builder&;

  constexpr auto access_expire(std::chrono::seconds d) -> cache_builder&;
  constexpr auto no_access_expire() noexcept -> cache_builder&;

  constexpr auto no_expire() noexcept -> cache_builder&;

  constexpr auto thread_safe() noexcept -> cache_builder&;
  constexpr auto not_thread_safe() noexcept -> cache_builder&;

  constexpr auto with_concurrency(unsigned int n = 0) noexcept -> cache_builder&;
  constexpr auto no_concurrency() noexcept -> cache_builder&;

  auto load_factor(float lf) -> cache_builder&;

  template<typename NewHash>
  auto with_hash(NewHash hash) const
  -> cache_builder<NewHash, Eq, Alloc>;

  template<typename NewEq>
  auto with_equality(NewEq eq) const
  -> cache_builder<Hash, NewEq, Alloc>;

  template<typename NewAlloc>
  auto with_allocator(NewAlloc alloc) const
  -> cache_builder<Hash, Eq, NewAlloc>;

  /**
   * \brief Build the cache described by this builder.
   * \note You should include <monsoon/cache_impl.h> if you call this function.
   * \returns A new cache described by the specification in this.
   */
  template<typename Fn>
  auto build(Fn&& fn) const
  -> cache<T, U>;

  auto max_memory() const noexcept -> std::optional<std::uintptr_t>;
  auto max_size() const noexcept -> std::optional<std::uintptr_t>;
  auto max_age() const noexcept -> std::optional<std::chrono::second>;
  auto access_expire() const noexcept -> std::optional<std::chrono::second>;
  auto alloc() const noexcept -> const Alloc&;
  auto thread_safe() const noexcept -> bool;
  auto concurrency() const noexcept -> unsigned int;
  auto load_factor() const noexcept -> float;
  auto hash() const noexcept -> Hash;
  auto equality() const noexcept -> Eq;
  auto allocator() const noexcept -> Alloc;

 private:
  std::optional<std::uintptr_t> max_memory_, max_size_;
  std::optional<std::chrono::seconds> max_age_, access_expire_;
  bool thread_safe_ = true;
  unsigned int concurrency_ = 0; // zero signifies to use std::hardware_concurrency
  Hash hash_;
  Eq eq_;
  Alloc alloc_;
};

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
constexpr cache_builder<T, U, Hash, Eq, Alloc>::cache_builder(Hash hash, Eq eq, Alloc alloc)
: hash_(std::move(hash_)),
  eq_(std::move(eq_)),
  alloc_(std::move(alloc))
{}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
template<typename OtherT, typename OtherU, typename OtherHash, typename OtherEq, typename OtherAlloc>
constexpr cache_builder<T, U, Hash, Eq, Alloc>::explicit cache_builder(
    const cache_builder<OtherT, OtherU, OtherHash, OtherEq, OtherAlloc>& other,
    Hash hash = Hash(), Eq eq = Eq(), Alloc alloc = Alloc())
: max_memory_(other.max_memory_),
  max_size_(other.max_size_),
  max_age_(other.max_age_),
  access_expire_(other.access_expire_),
  thread_safe_(other.thread_safe_),
  concurrency_(other.concurrency_),
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
  if (d <= std::chrono::seconds::zero)
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
  if (d <= std::chrono::seconds::zero)
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
constexpr auto cache_builder<T, U, Hash, Eq, Alloc>::with_concurrency(unsigned int n = 0)
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
-> cache_builder<NewHash, Eq, Alloc> {
  return cache_builder<NewHash, Eq, Alloc>(*this, hash, eq_, alloc_);
}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
template<typename NewEq>
auto cache_builder<T, U, Hash, Eq, Alloc>::with_equality(NewEq eq) const
-> cache_builder<Hash, NewEq, Alloc> {
  return cache_builder<Hash, NewEq, NewAlloc>(*this, hash_, eq, alloc_);
}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
template<typename NewAlloc>
auto cache_builder<T, U, Hash, Eq, Alloc>::with_allocator(NewAlloc alloc) const
-> cache_builder<Hash, Eq, NewAlloc> {
  return cache_builder<Hash, Eq, NewAlloc>(*this, hash_, eq_, alloc);
}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
auto cache_builder<T, U, Hash, Eq, Alloc>::max_memory() const
noexcept
-> std::optional<std::uintptr_t> {
  return max_memory_;
}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
auto cache_builder<T, U, Hash, Eq, Alloc>::max_size() const
noexcept
-> std::optional<std::uintptr_t> {
  return max_size_;
}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
auto cache_builder<T, U, Hash, Eq, Alloc>::max_age() const
noexcept
-> std::optional<std::chrono::second> {
  return max_age_;
}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
auto cache_builder<T, U, Hash, Eq, Alloc>::access_expire() const
noexcept
-> std::optional<std::chrono::second> {
  return access_expire_;
}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
auto cache_builder<T, U, Hash, Eq, Alloc>::alloc() const
noexcept
-> const Alloc& {
  return alloc;
}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
auto cache_builder<T, U, Hash, Eq, Alloc>::thread_safe() const
noexcept
-> bool {
  return thread_safe_;
}

template<typename T, typename U, typename Hash, typename Eq, typename Alloc>
auto cache_builder<T, U, Hash, Eq, Alloc>::concurrency() const
noexcept
-> unsigned int {
  return concurrency_;
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
