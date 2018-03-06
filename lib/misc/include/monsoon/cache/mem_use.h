#ifndef MONSOON_CACHE_MEM_USE_H
#define MONSOON_CACHE_MEM_USE_H

///\file
///\ingroup cache_detail

#include <atomic>
#include <monsoon/cache/allocator.h>

namespace monsoon::cache {


/**
 * \brief Memory usage tracking.
 * \details
 * Tracks memory usage by allocator.
 */
class mem_use
: public cache_alloc_dealloc_observer
{
 public:
  mem_use() = default;
  mem_use(const mem_use&) = delete;
  mem_use(mem_use&&) = delete;
  mem_use& operator=(const mem_use&) = delete;
  mem_use& operator=(mem_use&&) = delete;

  auto add_mem_use(std::uintptr_t n, std::uintptr_t sz)
  noexcept
  -> void
  override {
    mem_used_.fetch_add(n * sz, std::memory_order_relaxed);
  }

  auto subtract_mem_use(std::uintptr_t n, std::uintptr_t sz)
  noexcept
  -> void
  override {
    mem_used_.fetch_add(n * sz, std::memory_order_relaxed);
  }

  auto get() const
  noexcept
  -> std::uintptr_t {
    return mem_used_.load(std::memory_order_relaxed);
  }

 private:
  std::atomic<std::uintptr_t> mem_used_{ 0u };
};


} /* namespace monsoon::cache */

#endif /* MONSOON_CACHE_MEM_USE_H */
