#ifndef MONSOON_EXPR_RESULT_INL_H
#define MONSOON_EXPR_RESULT_INL_H

namespace monsoon {


inline expr_result::expr_result()
: data_(data_type::create<1>())
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
: data_(data_type::create<0>(std::move(v)))
{}

template<typename Iter>
expr_result::expr_result(Iter b, Iter e)
: data_(data_type::create<1>(b, e))
{}

inline auto expr_result::is_scalar() const noexcept -> bool {
  return data_.selector() == 0;
}

inline auto expr_result::is_vector() const noexcept -> bool {
  return data_.selector() == 1;
}

inline auto expr_result::empty() const noexcept -> bool {
  return map_onto<bool>(data_,
      [](const metric_value&) {
        return false;
      },
      [](const map_type& m) {
        return m.empty();
      });
}

inline auto expr_result::size() const noexcept -> size_type {
  return map_onto<size_type>(data_,
      [](const metric_value&) {
        return 1u;
      },
      [](const map_type& m) {
        return m.size();
      });
}

inline auto expr_result::as_scalar() const noexcept
->  optional<const metric_value&> {
  return map_onto<optional<const metric_value&>>(data_,
      [](const metric_value& v) -> const metric_value& {
        return v;
      },
      [](const map_type&) {
        return optional<const metric_value&>();
      });
}

inline auto expr_result::as_vector() const noexcept
->  optional<const map_type&> {
  return map_onto<optional<const map_type&>>(data_,
      [](const metric_value&) {
        return optional<const map_type&>();
      },
      [](const map_type& m) -> const map_type& {
        return m;
      });
}

inline auto expr_result::as_scalar() noexcept
->  optional<metric_value&> {
  return map_onto<optional<metric_value&>>(data_,
      [](metric_value& v) -> metric_value& {
        return v;
      },
      [](map_type&) {
        return optional<metric_value&>();
      });
}

inline auto expr_result::as_vector() noexcept
->  optional<map_type&> {
  return map_onto<optional<map_type&>>(data_,
      [](metric_value&) {
        return optional<map_type&>();
      },
      [](map_type& m) -> map_type& {
        return m;
      });
}

template<typename FN>
auto expr_result::transform_tags(FN fn)
    noexcept(noexcept(invoke(std::declval<FN>(),
                             std::declval<const tags&>())))
->  void {
  visit(data_,
      [&fn](metric_value&) {},
      [&fn](map_type& m) {
        map_type replacement;
        for (auto& v : m)
          replacement.emplace(invoke(fn, v.first), std::move(v.second));
        m = std::move(replacement);
      });
}

template<typename FN>
auto expr_result::transform_values(FN fn)
    noexcept(noexcept(invoke(std::declval<FN>(),
                             std::declval<metric_value>())))
->  void {
  visit(data_,
      [&fn](metric_value& v) {
        v = invoke(fn, std::move(v));
      },
      [&fn](map_type& m) {
        for (auto& v : m)
          v.second = invoke(fn, std::move(v.second));
      });
}

template<typename FN>
auto expr_result::filter_tags(FN fn)
    noexcept(noexcept(invoke(std::declval<FN>(),
                             std::declval<const tags&>())))
->  void {
  visit(data_,
      [](metric_value&) {},
      [&fn](map_type& m) {
        std::vector<map_type::iterator> to_be_removed;
        to_be_removed.reserve(m.size());

        for (map_type::iterator i = m.begin(); i != m.end(); ++i) {
          if (invoke(fn, i->first))
            to_be_removed.push_back(i);
        }

        for (map_type::iterator& i : to_be_removed) {
          m.erase(i);
        }
      });
}


} /* namespace monsoon */

#endif /* MONSOON_EXPR_RESULT_INL_H */
