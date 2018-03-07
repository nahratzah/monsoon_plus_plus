#ifndef MONSOON_SIMPLE_GROUP_INL_H
#define MONSOON_SIMPLE_GROUP_INL_H

#include <utility>
#include <cassert>
#include <algorithm>

namespace monsoon {


struct simple_group::cache_hasher_ {
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

struct simple_group::cache_eq_ {
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

struct simple_group::cache_create_ {
  template<typename Alloc, typename... Args>
  auto operator()(const Alloc& alloc, Args&&... args) const
  -> std::shared_ptr<path_type> {
    return std::allocate_shared<path_type>(alloc, std::forward<Args>(args)...);
  }
};


inline simple_group::simple_group()
: path_(cache_()())
{}

inline simple_group::simple_group(const path_type& p)
: path_(cache_()(p))
{}

template<typename T, typename Alloc>
inline simple_group::simple_group(const std::vector<T, Alloc>& p)
: simple_group(p.begin(), p.end())
{}

inline simple_group::simple_group(std::initializer_list<const char*> init)
: simple_group(init.begin(), init.end())
{}

inline simple_group::simple_group(std::initializer_list<std::string> init)
: simple_group(init.begin(), init.end())
{}

template<typename Iter>
inline simple_group::simple_group(Iter b, Iter e)
: simple_group(b, e, typename std::iterator_traits<Iter>::iterator_category())
{}

template<typename Iter>
inline simple_group::simple_group(Iter b, Iter e, std::input_iterator_tag) {
  path_type tmp = path_type(b, e);
  *this = simple_group(tmp.begin(), tmp.end());
}

template<typename Iter>
inline simple_group::simple_group(Iter b, Iter e, std::forward_iterator_tag)
: path_(cache_()(b, e))
{}

inline auto simple_group::get_path() const noexcept -> const path_type& {
  assert(path_ != nullptr);
  return *path_;
}

inline auto simple_group::begin() const noexcept -> iterator {
  assert(path_ != nullptr);
  return path_->begin();
}

inline auto simple_group::end() const noexcept -> iterator {
  assert(path_ != nullptr);
  return path_->end();
}

inline auto simple_group::operator!=(const simple_group& other) const noexcept
->  bool {
  return !(*this == other);
}

inline auto simple_group::operator>(const simple_group& other) const noexcept
->  bool {
  return other < *this;
}

inline auto simple_group::operator<=(const simple_group& other) const noexcept
->  bool {
  return !(other < *this);
}

inline auto simple_group::operator>=(const simple_group& other) const noexcept
->  bool {
  return !(*this < other);
}


} /* namespace monsoon */

#endif /* MONSOON_SIMPLE_GROUP_INL_H */
