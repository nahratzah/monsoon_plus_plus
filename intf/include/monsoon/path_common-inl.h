#ifndef MONSOON_PATH_COMMON_INL_H
#define MONSOON_PATH_COMMON_INL_H

#include <utility>
#include <cassert>
#include <algorithm>

namespace monsoon {


struct path_common::cache_hasher_ {
  constexpr auto operator()() const
  noexcept
  -> std::size_t {
    return 0;
  }

  auto operator()(const path_type& p) const
  noexcept
  -> std::size_t {
    return (*this)(p.begin(), p.end());
  }

  template<typename Iter>
  auto operator()(Iter b, Iter e) const
  noexcept
  -> std::size_t {
    static_assert(std::is_base_of_v<std::forward_iterator_tag, typename std::iterator_traits<Iter>::iterator_category>,
        "Iterator must be at least a forward iterator.");

    std::hash<std::string_view> elem_hasher;
    std::size_t rv = 0;
    while (b != e)
      rv = 19u * rv + elem_hasher(*b++);
    return rv;
  }
};

struct path_common::cache_eq_ {
  auto operator()(const path_type& p) const
  noexcept
  -> bool {
    return p.empty();
  }

  auto operator()(const path_type& k, const path_type& search) const
  noexcept
  -> bool {
    return std::equal(
        k.begin(), k.end(),
        search.begin(), search.end(),
        std::equal_to<std::string_view>());
  }

  template<typename Iter>
  auto operator()(const path_type& p, Iter b, Iter e) const
  noexcept
  -> bool {
    static_assert(std::is_base_of_v<std::forward_iterator_tag, typename std::iterator_traits<Iter>::iterator_category>,
        "Iterator must be at least a forward iterator.");

    return std::equal(
        p.begin(), p.end(),
        b, e,
        std::equal_to<std::string_view>());
  }
};

struct path_common::cache_create_ {
  template<typename Alloc, typename... Args>
  auto operator()(const Alloc& alloc, Args&&... args) const
  -> std::shared_ptr<path_type> {
    return std::allocate_shared<path_type>(alloc, std::forward<Args>(args)...);
  }
};


template<typename T, typename Alloc>
inline path_common::path_common(const std::vector<T, Alloc>& p)
: path_common(p.begin(), p.end())
{}

template<typename Iter>
inline path_common::path_common(Iter b, Iter e)
: path_common(b, e, typename std::iterator_traits<Iter>::iterator_category())
{}

template<typename Iter>
inline path_common::path_common(Iter b, Iter e, std::input_iterator_tag) {
  path_type tmp = path_type(b, e);
  *this = path_common(tmp);
}

template<typename Iter>
inline path_common::path_common(Iter b, Iter e, std::forward_iterator_tag)
: path_(cache_()(b, e))
{}

inline auto path_common::get_path() const noexcept -> const path_type& {
  assert(path_ != nullptr);
  return *path_;
}

inline auto path_common::begin() const noexcept -> const_iterator {
  assert(path_ != nullptr);
  return path_->begin();
}

inline auto path_common::end() const noexcept -> const_iterator {
  assert(path_ != nullptr);
  return path_->end();
}

inline auto path_common::cbegin() const noexcept -> const_iterator {
  assert(path_ != nullptr);
  return path_->begin();
}

inline auto path_common::cend() const noexcept -> const_iterator {
  assert(path_ != nullptr);
  return path_->end();
}

inline auto path_common::operator!=(const path_common& other) const noexcept
->  bool {
  return !(*this == other);
}

inline auto path_common::operator>(const path_common& other) const noexcept
->  bool {
  return other < *this;
}

inline auto path_common::operator<=(const path_common& other) const noexcept
->  bool {
  return !(other < *this);
}

inline auto path_common::operator>=(const path_common& other) const noexcept
->  bool {
  return !(*this < other);
}


} /* namespace monsoon */

#endif /* MONSOON_PATH_COMMON_INL_H */
