#include <monsoon/match_clauses/default_match_clause.h>
#include <functional>
#include <tuple>
#include <vector>

namespace monsoon {
namespace match_clauses {


default_match_clause::~default_match_clause() noexcept {}

auto default_match_clause::apply(
    expr_result x,
    expr_result y,
    std::function<metric_value(metric_value, metric_value)> fn) const
->  expr_result {
  using std::placeholders::_1;

  if (x.is_scalar() && y.is_scalar())
    return expr_result(fn(std::move(*x.as_scalar()),
                          std::move(*y.as_scalar())));
  if (x.is_scalar()) {
    y.transform_values(std::bind(std::move(fn),
                                 std::move(*x.as_scalar()), _1));
    return y;
  }
  if (y.is_scalar()) {
    x.transform_values(std::bind(std::move(fn),
                                 _1, std::move(*y.as_scalar())));
    return y;
  }

  std::vector<std::tuple<tags, metric_value>> result;

  if (x.size() < y.size()) {
    expr_result::map_type& y_map = *y.as_vector();
    for (expr_result::map_type::reference x_pair : *x.as_vector()) {
      auto y_pair_iter = y_map.find(x_pair.first);
      if (y_pair_iter == y_map.end()) continue;
      expr_result::map_type::reference y_pair = *y_pair_iter;

      result.emplace_back(x_pair.first,
                          fn(std::move(x_pair.second),
                             std::move(y_pair.second)));
    }
  } else {
    expr_result::map_type& x_map = *x.as_vector();
    for (expr_result::map_type::reference y_pair : *y.as_vector()) {
      auto x_pair_iter = x_map.find(y_pair.first);
      if (x_pair_iter == x_map.end()) continue;
      expr_result::map_type::reference x_pair = *x_pair_iter;

      result.emplace_back(y_pair.first,
                          fn(std::move(x_pair.second),
                             std::move(y_pair.second)));
    }
  }

  return expr_result(std::make_move_iterator(result.begin()),
                     std::make_move_iterator(result.end()));
}

auto default_match_clause::do_ostream(std::ostream&) const -> void {}


}} /* namespace monsoon::match_clauses */
