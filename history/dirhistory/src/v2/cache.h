#ifndef V2_CACHE_H
#define V2_CACHE_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include "../dynamics.h"
#include "file_segment_ptr.h"
#include <string_view>
#include <typeinfo>
#include <typeindex>
#include <memory>
#include <monsoon/cache/cache.h>
#include <monsoon/cache/allocator.h>
#include <monsoon/history/dir/hdir_exception.h>
#include <instrumentation/group.h>
#include <instrumentation/timing.h>
#include <instrumentation/time_track.h>

namespace monsoon::history::v2 {


template<typename, typename>
class monsoon_dirhistory_local_ cache_search_type;

template<typename T, typename P>
auto decode(const cache_search_type<T, P>& cst, typename T::allocator_type alloc)
-> std::shared_ptr<T> {
  auto xdr = cst.parent()->get_ctx().new_reader(cst.fptr(), T::is_compressed);
  auto result = std::allocate_shared<T>(alloc, std::const_pointer_cast<P>(cst.parent()), alloc);
  result->decode(xdr);
  if (!xdr.at_end()) throw dirhistory_exception("xdr data remaining");
  xdr.close();
  return result;
}

monsoon_dirhistory_local_ instrumentation::group& cache_grp();

template<typename T>
using cache_allocator = monsoon::cache::cache_allocator<std::allocator<T>>;

template<typename Type, typename Parent>
class monsoon_dirhistory_local_ cache_search_type {
 public:
  cache_search_type(std::shared_ptr<const Parent> parent, file_segment_ptr fptr) noexcept
  : parent_(std::move(parent)),
    fptr_(std::move(fptr))
  {}

  auto parent() const
  noexcept
  -> const std::shared_ptr<const Parent>& {
    return parent_;
  }

  auto fptr() const
  noexcept
  -> const file_segment_ptr& {
    return fptr_;
  }

  auto type() const
  noexcept
  -> std::type_index {
    return std::type_index(typeid(Type));
  }

 private:
  std::shared_ptr<const Parent> parent_;
  file_segment_ptr fptr_;
};

class monsoon_dirhistory_local_ cache_key_type {
 public:
  template<typename T, typename P>
  cache_key_type(const cache_search_type<T, P>& y) noexcept
  : parent_(y.parent()),
    fptr_(y.fptr()),
    type_(y.type())
  {}

  auto operator==(const cache_key_type& y) const
  noexcept
  -> bool {
    std::shared_ptr<const dynamics> x_parent = parent_.lock();
    std::shared_ptr<const dynamics> y_parent = y.parent_.lock();
    return x_parent == y_parent
        && fptr_ == y.fptr_
        && type_ == y.type_;
  }

  auto operator!=(const cache_key_type& y) const
  noexcept
  -> bool {
    return !(*this == y);
  }

  auto parent() const
  noexcept
  -> std::shared_ptr<const dynamics> {
    return parent_.lock();
  }

  auto fptr() const
  noexcept
  -> const file_segment_ptr& {
    return fptr_;
  }

  auto type() const
  noexcept
  -> const std::type_index& {
    return type_;
  }

 private:
  std::weak_ptr<const dynamics> parent_;
  file_segment_ptr fptr_;
  std::type_index type_;
};

struct monsoon_dirhistory_local_ dynamics_cache_hash {
  using parent_hash = std::hash<std::shared_ptr<const dynamics>>;
  using fptr_hash = std::hash<file_segment_ptr>;
  using type_hash = std::hash<std::type_index>;

  template<typename T>
  auto operator()(const T& cst) const
  noexcept
  -> std::size_t {
    return parent_hash()(cst.parent())
        ^ fptr_hash()(cst.fptr())
        ^ type_hash()(cst.type());
  }
};

struct monsoon_dirhistory_local_ dynamics_cache_equal {
  template<typename T, typename U>
  auto operator()(const T& x, const U& y) const
  noexcept
  -> bool {
    return x.parent() == y.parent()
        && x.fptr() == y.fptr()
        && x.type() == y.type()
        && x.parent() != nullptr
        && y.parent() != nullptr;
  }
};

struct monsoon_dirhistory_local_ dynamics_cache_create {
  template<typename Alloc, typename T, typename P>
  auto operator()(Alloc alloc, const cache_search_type<T, P>& cst) const
  -> std::shared_ptr<dynamics> {
    instrumentation::time_track<instrumentation::timing> track_{ decode_duration<T> };
    return decode(cst, alloc);
  }

  template<typename Alloc>
  auto operator()(const Alloc& alloc, const cache_key_type& cst) const
  -> std::shared_ptr<dynamics> {
    throw std::invalid_argument("cache key type is not suitable to create items");
  }

 private:
  template<typename T>
  static inline instrumentation::timing decode_duration{ "timing_duration", cache_grp(), instrumentation::tag_map({ {"type", std::string_view(typeid(T).name())} }) };
};

using cache_type = monsoon::cache::extended_cache<
    cache_key_type,
    dynamics,
    dynamics_cache_hash,
    dynamics_cache_equal,
    cache_allocator<dynamics>,
    dynamics_cache_create>;

monsoon_dirhistory_local_
auto get_dynamics_cache_() -> cache_type&;

template<typename Type, typename Parent>
auto get_dynamics_cache(
    std::shared_ptr<Parent> parent,
    file_segment_ptr fptr)
-> std::shared_ptr<Type> {
  using search_type = cache_search_type<std::decay_t<Type>, std::decay_t<Parent>>;

  return std::dynamic_pointer_cast<Type>(
      get_dynamics_cache_().get(
          search_type(std::move(parent), std::move(fptr))));
}


} /* namespace monsoon::history::v2 */

#endif /* V2_CACHE_H */
