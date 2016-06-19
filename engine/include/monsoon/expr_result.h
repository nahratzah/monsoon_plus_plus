#ifndef MONSOON_EXPR_RESULT_H
#define MONSOON_EXPR_RESULT_H

#include <monsoon/any.h>
#include <monsoon/optional.h>
#include <monsoon/invoke.h>
#include <monsoon/metric_value.h>
#include <monsoon/tags.h>
#include <unordered_map>

namespace monsoon {


class expr_result {
 public:
  using map_type = std::unordered_map<tags, metric_value>;
  using size_type = map_type::size_type;

 private:
  using data_type = any<metric_value, map_type>;

 public:
  expr_result();
  expr_result(const expr_result&) = default;
  expr_result(expr_result&&) noexcept;
  expr_result& operator=(const expr_result&) = default;
  expr_result& operator=(expr_result&&) noexcept;

  expr_result(metric_value) noexcept;
  template<typename Iter> expr_result(Iter, Iter);

  bool is_scalar() const noexcept;
  bool is_vector() const noexcept;
  bool empty() const noexcept;
  size_type size() const noexcept;
  optional<const metric_value&> as_scalar() const noexcept;
  optional<const map_type&> as_vector() const noexcept;
  optional<metric_value&> as_scalar() noexcept;
  optional<map_type&> as_vector() noexcept;

  template<typename FN> void transform_tags(FN)
      noexcept(noexcept(invoke(std::declval<FN>(),
                               std::declval<const tags&>())));
  template<typename FN> void transform_values(FN)
      noexcept(noexcept(invoke(std::declval<FN>(),
                               std::declval<metric_value>())));
  template<typename FN> void filter_tags(FN)
      noexcept(noexcept(invoke(std::declval<FN>(),
                               std::declval<const tags&>())));

 private:
  any<metric_value, map_type> data_;
};


} /* namespace monsoon */

#include "expr_result-inl.h"

#endif /* MONSOON_EXPR_RESULT_H */
