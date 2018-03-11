#ifndef MONSOON_TAGS_INL_H
#define MONSOON_TAGS_INL_H

#include <cassert>
#include <utility>
#include <map>
#include <unordered_map>
#include <algorithm>

namespace monsoon {


struct tags::cache_hasher_ {
  constexpr auto operator()() const
  noexcept
  -> std::size_t {
    return 0;
  }

  template<typename Collection>
  auto operator()(const Collection& collection) const
  noexcept
  -> std::size_t {
    return (*this)(collection.begin(), collection.end());
  }

  template<typename Iter>
  auto operator()(Iter b, Iter e) const
  noexcept
  -> std::size_t {
    static_assert(std::is_base_of_v<std::forward_iterator_tag, typename std::iterator_traits<Iter>::iterator_category>,
        "Iterator must be at least a forward iterator.");

    std::hash<std::string_view> key_hasher;
    std::hash<metric_value> mapped_hasher;
    std::size_t rv = 0;
    while (b != e) {
      rv ^= 23u * key_hasher(std::get<0>(*b)) + mapped_hasher(std::get<1>(*b));
      ++b;
    }
    return rv;
  }
};

struct tags::cache_eq_ {
  auto operator()(const map_type& p) const
  noexcept
  -> bool {
    return p.empty();
  }

  template<typename K, typename V, typename Alloc>
  auto operator()(const map_type& k, const std::map<K, V, std::less<>, Alloc>& search) const
  noexcept
  -> std::enable_if_t<
      std::is_convertible_v<K, std::string_view>
      && std::is_convertible_v<V, metric_value>,
      bool> {
    return std::equal(
        k.begin(), k.end(),
        search.begin(), search.end(),
        [](const auto& x, const auto& y) { return elem_eq_predicate(x, y); });
  }

  template<typename K, typename V, typename Less, typename Alloc>
  auto operator()(const map_type& k, const std::map<K, V, Less, Alloc>& search) const
  noexcept
  -> bool {
    for (const auto& e : search) {
      auto k_pos = k.find(std::string_view(std::get<0>(e)));
      if (k_pos == k.end()) return false;
      if (!elem_eq_predicate(e, *k_pos)) return false;
    }
    return true;
  }

  template<typename K, typename V, typename H, typename E, typename Alloc>
  auto operator()(const map_type& k, const std::unordered_map<K, V, H, E, Alloc>& search) const
  noexcept
  -> bool {
    for (const auto& e : search) {
      const auto k_pos = k.find(std::string_view(std::get<0>(e)));
      if (k_pos == k.end()
          || !elem_eq_predicate(e, *k_pos))
        return false;
    }
    return true;
  }

  template<typename Collection>
  auto operator()(const map_type& k, const Collection& collection) const
  noexcept
  -> bool {
    return (*this)(k, collection.begin(), collection.end());
  }

  template<typename Iter>
  auto operator()(const map_type& p, Iter b, Iter e) const
  noexcept
  -> bool {
    static_assert(std::is_base_of_v<std::forward_iterator_tag, typename std::iterator_traits<Iter>::iterator_category>,
        "Iterator must be at least a forward iterator.");

    if (std::is_sorted(b, e,
            [](const auto& x, const auto& y) {
              return std::less<std::string_view>()(std::get<0>(x), std::get<0>(y));
            })) {
      return std::equal(
          p.begin(), p.end(),
          b, e,
          [](const auto& x, const auto& y) { return elem_eq_predicate(x, y); });
    }

    std::vector<std::pair<std::string_view, metric_value>> tmp;
    if constexpr(std::is_base_of_v<std::random_access_iterator_tag, typename std::iterator_traits<Iter>::iterator_category>)
      tmp.reserve(e - b);
    std::copy(b, e, std::back_inserter(tmp));
    std::sort(
        tmp.begin(), tmp.end(),
        [](const auto& x, const auto& y) { return x.first < y.first; });
    return std::equal(
        p.begin(), p.end(),
        tmp.begin(), tmp.end(),
        [](const auto& x, const auto& y) { return elem_eq_predicate(x, y); });
  }

 private:
  template<typename X, typename Y>
  static auto elem_eq_predicate(const X& x, const Y& y) {
    std::string_view x_key = std::get<0>(x);
    std::string_view y_key = std::get<0>(y);
    if (x_key != y_key) return false;

    const metric_value& x_val = std::get<1>(x);
    const metric_value& y_val = std::get<1>(y);
    if (x_val != y_val) return false;

    return true;
  }
};

struct tags::cache_create_ {
  template<typename Alloc, typename... Args>
  auto operator()(const Alloc& alloc, Args&&... args) const
  -> std::shared_ptr<map_type> {
    return std::allocate_shared<map_type>(alloc, std::forward<Args>(args)...);
  }

  template<typename Alloc, typename Arg>
  auto operator()(const Alloc& alloc, Arg&& arg) const
  -> std::enable_if_t<
      !std::is_constructible_v<map_type, Arg>,
      std::shared_ptr<map_type>> {
    return std::allocate_shared<map_type>(alloc, arg.begin(), arg.end());
  }
};


template<typename Iter>
tags::tags(Iter b, Iter e)
: tags(b, e, typename std::iterator_traits<Iter>::iterator_category())
{}

template<typename Collection>
tags::tags(const Collection& collection)
: map_(cache_()(collection))
{}

template<typename Iter>
tags::tags(Iter b, Iter e, [[maybe_unused]] std::input_iterator_tag tag)
: tags(map_type(b, e))
{}

template<typename Iter>
tags::tags(Iter b, Iter e, [[maybe_unused]] std::forward_iterator_tag tag)
: map_(cache_()(b, e))
{}

inline auto tags::empty() const noexcept -> bool {
  assert(map_ != nullptr);
  return map_->empty();
}

inline auto tags::size() const noexcept -> std::size_t {
  assert(map_ != nullptr);
  return map_->size();
}

inline auto tags::begin() const noexcept -> iterator {
  assert(map_ != nullptr);
  return map_->begin();
}

inline auto tags::end() const noexcept -> iterator {
  assert(map_ != nullptr);
  return map_->end();
}

inline auto tags::operator!=(const tags& other) const noexcept -> bool {
  return !(*this == other);
}

inline auto tags::operator>(const tags& other) const noexcept -> bool {
  return other < *this;
}

inline auto tags::operator<=(const tags& other) const noexcept -> bool {
  return !(other < *this);
}

inline auto tags::operator>=(const tags& other) const noexcept -> bool {
  return !(*this < other);
}

template<typename Iter>
auto tags::has_keys(Iter b, Iter e) const -> bool {
  const map_type& map = *map_;
  return std::all_of(b, e,
      [&map](std::string_view s) { return map.find(s) != map.end(); });
}


} /* namespace monsoon */

#endif /* MONSOON_TAGS_INL_H */
