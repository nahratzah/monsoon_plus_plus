#include <monsoon/match_clauses/default_match_clause.h>

namespace monsoon {
namespace match_clauses {


default_match_clause::~default_match_clause() noexcept {}

auto default_match_clause::apply(
    std::unordered_map<tags, metric_value> x,
    std::unordered_map<tags, metric_value> y,
    std::function<metric_value(metric_value, metric_value)> fn) const
->  std::unordered_map<tags, metric_value> {
  std::unordered_map<tags, metric_value> result;

  if (x.size() <= y.size()) {
    for (const auto& x_pair : x) {
      auto y_pair_iter = y.find(x_pair.first);
      if (y_pair_iter == x.end()) continue;
      auto y_pair = *y_pair_iter;

      result.emplace(x_pair.first,
                     fn(std::move(x_pair.second), std::move(y_pair.second)));
    }
  } else {
    for (const auto& y_pair : y) {
      auto x_pair_iter = x.find(y_pair.first);
      if (x_pair_iter == x.end()) continue;
      auto x_pair = *x_pair_iter;

      result.emplace(x_pair.first,
                     fn(std::move(x_pair.second), std::move(y_pair.second)));
    }
  }

  return result;
}

auto default_match_clause::do_ostream(std::ostream&) const -> void {}


}} /* namespace monsoon::match_clauses */
