#ifndef MONSOON_CACHE_BUCKET_H
#define MONSOON_CACHE_BUCKET_H

#include <monsoon/cache/element.h>

namespace monsoon::cache {


/**
 * \brief Variables used by bucket to do its work.
 * \details These variables are kept outside bucket,
 * in order to reduce memory overhead.
 *
 * This essentially functions as the query to a bucket lookup.
 *
 * \tparam KeyEq A key predicate functor.
 * \code
 *  predicate(const store_type&) -> bool
 * \endcode
 * \tparam Create Constructor function.
 *  create() -> store_type.
 */
template<typename Predicate, typename Create, typename SizeType>
struct bucket_ctx {
  constexpr bucket_ctx(std::size_t hash_code, Predicate predicate, Create create, SizeType& size)
  noexcept(std::is_nothrow_move_constructible_v<Predicate>
      && std::is_nothrow_move_constructible_v<Create>)
  : hash_code(std::move(hash_code)),
    predicate(std::move(predicate)),
    create(std::move(create)),
    size(size)
  {}

  std::size_t hash_code;
  Predicate predicate; ///<\brief Element predicate.
  Create create; ///<\brief Constructor.
  SizeType& size; ///<\brief Reference to cache size.
};

template<typename Predicate, typename Create, typename SizeType>
constexpr auto make_bucket_ctx(std::size_t hash_code, Predicate&& predicate, Create&& create, SizeType& size)
-> bucket_ctx<std::decay_t<Predicate>, std::decay_t<Create>, SizeType> {
  using type = bucket_ctx<std::decay_t<Predicate>, std::decay_t<Create>, SizeType>;
  return type(hash_code,
      std::forward<Predicate>(predicate),
      std::forward<Create>(create),
      size);
}

template<typename T, typename... Decorators>
class bucket {
 private:
  struct bucket_link;

 public:
  using store_type = element<T, bucket_link, Decorators...>;
  using pointer = typename store_type::pointer;
  using lookup_type = typename store_type::ptr_return_type;

  constexpr bucket() noexcept = default;
  bucket(const bucket&) = delete;
  bucket& operator=(const bucket&) = delete;

#ifndef NDEBUG // Add nullptr assertion if in debug mode.
  ~bucket() noexcept {
    assert(head_ == nullptr);
  }

  constexpr bucket(bucket&& x) noexcept
  : head_(std::exchange(x.head_, nullptr))
  {}

  constexpr bucket& operator=(bucket&& x) noexcept {
    std::swap(head_, x.head_);
    return *this;
  }
#else // If not in debug mode, allow more optimizations.
  constexpr bucket(bucket&&) noexcept = default;
  constexpr bucket& operator=(bucket&&) noexcept = default;
#endif

  /**
   * \brief Look up element with given key.
   * \returns Element with the given key, or nullptr if no such element exists.
   */
  template<typename KeyPredicate, typename Create, typename SizeType>
  auto lookup_if_present(const bucket_ctx<KeyPredicate, Create, SizeType>& ctx) const
  noexcept
  -> lookup_type {
    const store_type* s = head_;
    while (s != nullptr) {
      if (s->hash() == ctx.hash_code
          && ctx.predicate(*s)) {
        // We don't check s.is_expired(), since the key can expire between
        // the check and pointer resolution.
        lookup_type ptr = s->ptr();
        if (!store_type::is_nil(ptr)) return ptr;
      }

      s = successor_ptr(*s);
    }
    return nullptr;
  }

  /**
   * \brief Look up or create element with given key.
   * \details
   * Iterates over all elements in the bucket.
   * While iterating, erases any expired entries.
   * \params[in] ctx Query context.
   * \params[out] created Set to point at newly created entry.
   *  Will be set to nullptr if the element was found.
   * \returns Element with the given key, creating one if absent.
   */
  template<typename Alloc, typename KeyPredicate, typename Create, typename SizeType>
  auto lookup_or_create(Alloc& use_alloc, const bucket_ctx<KeyPredicate, Create, SizeType>& ctx, store_type*& created)
  -> pointer {
    using allocator_type = typename std::allocator_traits<Alloc>::template rebind_alloc<store_type>;
    using alloc_traits = typename std::allocator_traits<Alloc>::template rebind_traits<store_type>;
    using size_type = typename alloc_traits::size_type;
    allocator_type alloc = use_alloc;

    created = nullptr; // Initialization of output argument.
    void* alloc_hint = nullptr; // Address of predecessor.
    store_type** iter = &head_; // Essentially a before-iterator, like in std::forward_list.

    while (*iter != nullptr) {
      store_type* s = *iter;

      // Clean up expired entries as we traverse.
      if (s->is_expired()) {
        *iter = successor_ptr(*s); // Unlink s.
        --ctx.size;

        alloc_traits::destroy(alloc, s);
        alloc_traits::deallocate(alloc, s, 1);
        continue;
      }

      alloc_hint = s; // Allocation hint update.
      if (s->hash() == ctx.hash_code && ctx.predicate(*s)) {
        lookup_type ptr = s->ptr();
        // Must check for nullptr: could have expired
        // since s->is_expired() check above.
        if (!store_type::is_nil(ptr)) return ptr;
      }

      iter = &successor_ptr(*s); // Advance iter.
    }

    // Create new store_type.
    store_type* new_store = alloc_traits::allocate(alloc, 1, alloc_hint);
    try {
      alloc_traits::construct(alloc, new_store, ctx.create()); // ctx.create may destructively access ctx.
    } catch (...) {
      alloc_traits::deallocate(alloc, new_store, 1);
      throw;
    }
    *iter = new_store; // Link.
    created = new_store; // Inform called of newly constructed store.
    ++ctx.size;
    lookup_type new_ptr = new_store->ptr();

    assert(!store_type::is_nil(new_ptr)
        && new_store->hash() == ctx.hash_code);
    return new_ptr;
  }

  /**
   * \brief Erases the element pointing at sptr.
   * \details
   * Loops over elements, find the non-expired element with plain pointer
   * equal to sptr.
   * This element is unlinked and destroyed.
   * \returns The result of element::ptr(), for the erased element.
   *  By returning the (potentially shared) reference, we allow the caller to
   *  delay destruction until after any locks have been released.
   */
  template<typename Alloc, typename SizeType>
  auto erase(Alloc& use_alloc, typename store_type::simple_pointer sptr, SizeType& size)
  noexcept
  -> lookup_type {
    using allocator_type = typename std::allocator_traits<Alloc>::template rebind_alloc<store_type>;
    using alloc_traits = typename std::allocator_traits<Alloc>::template rebind_traits<store_type>;
    allocator_type alloc = use_alloc;
    assert(sptr != nullptr);

    store_type** iter = &head_; // Essentially a before-iterator, like in std::forward_list.
    while (*iter != nullptr) {
      store_type*const s = *iter;

      if (s->is_ptr(sptr)) {
        *iter = successor_ptr(*s);
        lookup_type result = s->ptr();
        --size;

        alloc_traits::destroy(alloc, s);
        alloc_traits::deallocate(alloc, s, 1);
        if (!store_type::is_nil(result)) return result;
      } else {
        iter = &successor_ptr(*s);
      }
    }
    return lookup_type();
  }

  template<typename Alloc, typename SizeType>
  auto erase_all(Alloc& use_alloc, SizeType& size) {
    using allocator_type = typename std::allocator_traits<Alloc>::template rebind_alloc<store_type>;
    using alloc_traits = typename std::allocator_traits<Alloc>::template rebind_traits<store_type>;
    allocator_type alloc = use_alloc;

    while (head_ != nullptr) {
      store_type* s = std::exchange(head_, successor_ptr(*head_));
      --size;

      alloc_traits::destroy(alloc, s);
      alloc_traits::deallocate(alloc, s, 1);
    }
  }

  ///\brief Rehashes each element into the bucket found by bucket_lookup_fn.
  ///\params[in] bucket_lookup_fn A functor returning a reference to a destination bucket, based on a hash code.
  template<typename BucketLookupFn>
  auto rehash(BucketLookupFn bucket_lookup_fn)
  noexcept
  -> void {
    store_type** iter = &head_; // Essentially a before-iterator, like in std::forward_list.
    while (*iter != nullptr) {
      store_type*const s = *iter;

      bucket& dst = bucket_lookup_fn(s->hash());
      if (&dst == this) {
        iter = &successor_ptr(*s);
        continue;
      }

      *iter = successor_ptr(*s); // Unlink from this.
      successor_ptr(*s) = dst.head_; // Link chain of dst after s.
      dst.head_ = s; // Link s into dst.
    }
  }

 private:
  ///\brief The same as s.successor, but ensures bucket_link access.
  ///\returns Reference to the successor pointer of s.
  static auto successor_ptr(bucket_link& s)
  noexcept
  -> store_type*& {
    return s.successor;
  }

  ///\brief The same as s.successor, but ensures bucket_link access.
  ///\returns Reference to the successor pointer of s.
  static auto successor_ptr(const bucket_link& s)
  noexcept
  -> store_type*const& {
    return s.successor;
  }

  store_type* head_ = nullptr;
};

template<typename T, typename... Decorators>
class bucket<T, Decorators...>::bucket_link {
  template<typename, typename...> friend class bucket;

 public:
  template<typename Alloc, typename Ctx>
  constexpr bucket_link(
      [[maybe_unused]] std::allocator_arg_t,
      [[maybe_unused]] const Alloc& alloc,
      [[maybe_unused]] const Ctx& ctx)
  {}

 private:
  store_type* successor = nullptr; // Only accessible by bucket.
};


} /* namespace monsoon::cache */

#endif /* MONSOON_CACHE_BUCKET_H */
