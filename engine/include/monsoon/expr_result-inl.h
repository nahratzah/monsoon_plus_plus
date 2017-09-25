#ifndef MONSOON_EXPR_RESULT_INL_H
#define MONSOON_EXPR_RESULT_INL_H

#include <type_traits>
#include <algorithm>
#include <cassert>
#include <functional>

namespace monsoon {


inline expr_result::expr_result()
: data_(std::in_place_type<map_type>)
{}

inline expr_result::expr_result(expr_result&& other) noexcept
: data_(std::move(other.data_))
{}

inline auto expr_result::operator=(expr_result&& other) noexcept
->  expr_result& {
  data_ = std::move(other.data_);
  return *this;
}

inline expr_result::expr_result(metric_value v) noexcept
: data_(std::in_place_type<metric_value>, std::move(v))
{}

template<typename Iter>
expr_result::expr_result(Iter b, Iter e)
: data_(std::in_place_type<map_type>, b, e)
{}

inline auto expr_result::is_scalar() const noexcept -> bool {
  return data_.index() == 0;
}

inline auto expr_result::is_vector() const noexcept -> bool {
  return data_.index() == 1;
}

inline auto expr_result::empty() const noexcept -> bool {
  return visit(
      [](const auto& v) {
        using v_type = std::decay_t<decltype(v)>;

        if constexpr(std::is_same_v<metric_value, v_type>)
          return false;
        else
          return v.empty();
      },
      data_);
}

inline auto expr_result::size() const noexcept -> size_type {
  return visit(
      [](const auto& v) -> size_type {
        using v_type = std::decay_t<decltype(v)>;

        if constexpr(std::is_same_v<metric_value, v_type>)
          return 1u;
        else
          return v.size();
      },
      data_);
}

inline auto expr_result::as_scalar() const noexcept
->  const metric_value* {
  if (!is_scalar()) return nullptr;
  return &std::get<metric_value>(data_);
}

inline auto expr_result::as_vector() const noexcept
->  const map_type* {
  if (!is_vector()) return nullptr;
  return &std::get<map_type>(data_);
}

inline auto expr_result::as_scalar() noexcept
->  metric_value* {
  if (!is_scalar()) return nullptr;
  return &std::get<metric_value>(data_);
}

inline auto expr_result::as_vector() noexcept
->  map_type* {
  if (!is_vector()) return nullptr;
  return &std::get<map_type>(data_);
}

template<typename FN>
auto expr_result::transform_tags(FN fn)
    noexcept(noexcept(std::invoke(std::declval<FN>(),
                                  std::declval<const tags&>())))
->  void {
  visit(
      [&fn](auto& v) {
        using v_type = std::decay_t<decltype(v)>;

        if constexpr(std::is_same_v<metric_value, v_type>) {
          // SKIP
        } else if constexpr(std::is_same_v<map_type, v_type>) {
          map_type replacement;
          for (auto& entry : v)
            replacement.emplace(std::invoke(fn, entry.first), std::move(entry.second));
          v = std::move(replacement);
        } else {
          assert(false); // Unreachable.
        }
      },
      data_);
}

template<typename FN>
auto expr_result::transform_values(FN fn)
    noexcept(noexcept(std::invoke(std::declval<FN>(),
                                  std::declval<metric_value>())))
->  void {
  visit(
      [&fn](auto& v) {
        using v_type = std::decay_t<decltype(v)>;

        if constexpr(std::is_same_v<metric_value, v_type>) {
          v = std::invoke(fn, std::move(v));
        } else if constexpr(std::is_same_v<map_type, v_type>) {
          std::for_each(v.begin(), v.end(),
              [&fn](map_type::value_type& pair) {
                pair.second = std::invoke(fn, std::move(pair.second));
              });
        } else {
          assert(false); // Unreachable.
        }
      },
      data_);
}

template<typename FN>
auto expr_result::filter_tags(FN fn)
    noexcept(noexcept(std::invoke(std::declval<FN>(),
                                  std::declval<const tags&>())))
->  void {
  visit(
      [&fn](auto& v) {
        using v_type = std::decay_t<decltype(v)>;

        if constexpr(std::is_same_v<metric_value, v_type>) {
          // SKIP
        } else if constexpr(std::is_same_v<map_type, v_type>) {
          std::vector<map_type::iterator> to_be_removed;
          to_be_removed.reserve(v.size());

          for (map_type::iterator i = v.begin(); i != v.end(); ++i) {
            if (std::invoke(fn, i->first))
              to_be_removed.push_back(i);
          }

          for (map_type::iterator& i : to_be_removed)
            v.erase(i);
        } else {
          assert(false); // Unreachable.
        }
      },
      data_);
}


} /* namespace monsoon */

#endif /* MONSOON_EXPR_RESULT_INL_H */
