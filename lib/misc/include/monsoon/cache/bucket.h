#ifndef MONSOON_CACHE_BUCKET_H
#define MONSOON_CACHE_BUCKET_H

///\file
///\ingroup cache_detail

#include <monsoon/cache/element.h>
#include <monsoon/cache/store_delete_lock.h>

namespace monsoon::cache {


/**
 * \brief Variables used by bucket to do its work.
 * \ingroup cache_detail
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
 *
 * \bug Bucket_ctx contains a size reference, but the on_create and on_delete
 * functions in cache_impl should do this instead.
 */
template<typename Predicate, typename Create, typename OnHit, typename OnDelete>
struct bucket_ctx {
  constexpr bucket_ctx(std::size_t hash_code, Predicate predicate, Create create, OnHit on_hit, OnDelete on_delete)
  noexcept(std::is_nothrow_move_constructible_v<Predicate>
      && std::is_nothrow_move_constructible_v<Create>)
  : hash_code(std::move(hash_code)),
    predicate(std::move(predicate)),
    create(std::move(create)),
    on_hit(std::move(on_hit)),
    on_delete(std::move(on_delete))
  {}

  std::size_t hash_code;
  Predicate predicate; ///<\brief Element predicate.
  Create create; ///<\brief Constructor.
  OnHit on_hit; ///<\brief Invoked on cache hit.
  OnDelete on_delete; ///<\brief Invoked prior to deletion of element.
};

template<typename Predicate, typename Create, typename OnHit, typename OnDelete>
constexpr auto make_bucket_ctx(std::size_t hash_code, Predicate&& predicate, Create&& create, OnHit&& on_hit, OnDelete&& on_delete)
-> bucket_ctx<std::decay_t<Predicate>, std::decay_t<Create>, std::decay_t<OnHit>, std::decay_t<OnDelete>> {
  using type = bucket_ctx<std::decay_t<Predicate>, std::decay_t<Create>, std::decay_t<OnHit>, std::decay_t<OnDelete>>;
  return type(
      hash_code,
      std::forward<Predicate>(predicate),
      std::forward<Create>(create),
      std::forward<OnHit>(on_hit),
      std::forward<OnDelete>(on_delete));
}

/**
 * \brief Cache bucket.
 * \ingroup cache_detail
 * \details A bucket contains all objects, based on a given modulo of the hash code.
 */
template<typename T, typename... Decorators>
class bucket {
 private:
  class bucket_link;

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
   * \param[in] hash_code The hash code of the object to search.
   * \param[in] predicate Predicate matching the object to search.
   * \returns Element with the given key, or nullptr if no such element exists.
   */
  template<typename Predicate>
  auto lookup_if_present(std::size_t hash_code, Predicate predicate) const
  noexcept
  -> lookup_type {
    store_type* s = head_;
    while (s != nullptr) {
      if (s->hash() == hash_code && predicate(*s)) {
        // We don't check s.is_expired(), since the key can expire between
        // the check and pointer resolution.
        lookup_type ptr = s->ptr();
        if (!store_type::is_nil(ptr))
          return ptr;
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
   * \bug Instead of using a pointer-reference for \p created, a reference to the deletion lock should be supplied.
   */
  template<typename KeyPredicate, typename Create, typename OnHit, typename OnDelete>
  auto lookup_or_create(const bucket_ctx<KeyPredicate, Create, OnHit, OnDelete>& ctx, store_delete_lock<store_type>& created)
  -> lookup_type {
    assert(!created); // Lock must be supplied in empty state.
    void* alloc_hint = nullptr; // Address of predecessor.
    store_type** iter = &head_; // Essentially a before-iterator, like in std::forward_list.

    while (*iter != nullptr) {
      store_type* s = *iter;

      // Clean up expired entries as we traverse.
      if (s->is_expired()) {
        // Skip deletion if the use counter indicates the element is referenced.
        if (s->use_count.load(std::memory_order_acquire) != 0u) {
          // We don't update the allocation hint, as this element will likely disappear next time.
          iter = &successor_ptr(*s); // Advance iter.
          continue;
        }

        *iter = successor_ptr(*s); // Unlink s.

        ctx.on_delete(*s);
        continue;
      }

      alloc_hint = s; // Allocation hint update.
      if (s->hash() == ctx.hash_code && ctx.predicate(*s)) {
        lookup_type ptr = s->ptr();
        // Must check for nullptr: could have expired
        // since s->is_expired() check above.
        if (!store_type::is_nil(ptr)) {
          store_delete_lock<store_type> slck{ s };
          ctx.on_hit(*s);
          return ptr;
        }
      }

      iter = &successor_ptr(*s); // Advance iter.
    }

    // Create new store_type.
    store_type* new_store = ctx.create(alloc_hint);
    created = store_delete_lock<store_type>(new_store); // Inform called of newly constructed store.
    *iter = new_store; // Link.
    lookup_type new_ptr = new_store->ptr();

    assert(!store_type::is_nil(new_ptr)
        && new_store->hash() == ctx.hash_code);
    return new_ptr;
  }

  /**
   * \brief Erases the element sptr.
   * \details
   * The given store_type is removed from the bucket.
   * \pre sptr is a valid element of this bucket and is not locked against delete.
   * \post sptr will have been deleted from this bucket and deallocated.
   */
  template<typename OnDelete>
  auto erase(store_type* sptr, OnDelete on_delete)
  noexcept
  -> void {
    assert(sptr != nullptr);

    store_type** iter = &head_; // Essentially a before-iterator, like in std::forward_list.
    while (*iter != sptr) {
      assert(*iter != nullptr); // Only valid sptr may be supplied, so we'll never reach past the end of the bucket.
      iter = &successor_ptr(**iter);
    }

    store_type*const s = *iter;
    assert(s != nullptr);
    assert(s->use_count == 0u);
    *iter = successor_ptr(*s);

    on_delete(*s);
  }

  template<typename OnDelete>
  auto erase_all(OnDelete on_delete) {
    while (head_ != nullptr) {
      store_type* s = std::exchange(head_, successor_ptr(*head_));
      assert(s->use_count == 0u);

      on_delete(*s);
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

/**
 * \brief Element decorator used by bucket to maintain its chain of elements.
 * \ingroup cache_detail
 */
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
