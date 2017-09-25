#include <monsoon/match_clauses/by_clause.h>
#include <monsoon/config_support.h>
#include <algorithm>
#include <functional>
#include <iterator>
#include <tuple>
#include <ostream>
#include <vector>

namespace monsoon {
namespace match_clauses {


by_clause::~by_clause() noexcept {}

auto by_clause::apply(
    expr_result x,
    expr_result y,
    std::function<metric_value(metric_value, metric_value)> fn) const
->  expr_result {
  using std::placeholders::_1;

  // Special case both scalars.
  if (x.is_scalar() && y.is_scalar())
    return expr_result(fn(std::move(*x.as_scalar()),
                          std::move(*y.as_scalar())));

  // Special case if one of x or y is a scalar.
  if (x.is_scalar()) {
    y.filter_tags([this](const tags& t) {
                    return t.has_keys(this->tag_names_.begin(),
                                      this->tag_names_.end());
                  });
    if (!keep_common_) {
      y.transform_tags(std::bind([](std::optional<tags>&& o) {
                                   return std::move(o).value();
                                 },
                                 std::bind(&by_clause::reduce_tag_,
                                           this, _1)));
    }
    y.transform_values(std::bind(fn, std::move(*x.as_scalar()), _1));
    return y;
  }
  if (y.is_scalar()) {
    x.filter_tags([this](const tags& t) {
                    return t.has_keys(this->tag_names_.begin(),
                                      this->tag_names_.end());
                  });
    if (!keep_common_) {
      x.transform_tags(std::bind([](std::optional<tags>&& o) {
                                   return std::move(o).value();
                                 },
                                 std::bind(&by_clause::reduce_tag_,
                                           this, _1)));
    }
    x.transform_values(std::bind(fn, _1, std::move(*y.as_scalar())));
    return x;
  }

  // x and y are both vectors.
  std::vector<std::tuple<tags, metric_value>> result;
  const mapping x_mapping = map_(*x.as_vector());
  const mapping y_mapping = map_(*y.as_vector());

  auto x_iter = x_mapping.begin();
  auto y_iter = y_mapping.begin();
  while (x_iter != x_mapping.end() && y_iter != y_mapping.end()) {
    if (x_iter->first < y_iter->first) {
      ++x_iter;
    } else if (y_iter->first < x_iter->first) {
      ++y_iter;
    } else {
      result.emplace_back(select_tags_(x_iter->first,
                                       x_iter->second->first,
                                       y_iter->second->first),
                          fn(std::move(x_iter->second->second),
                             std::move(y_iter->second->second)));
      ++x_iter;
      ++y_iter;
    }
  }

  return expr_result(std::make_move_iterator(result.begin()),
                     std::make_move_iterator(result.end()));
}

auto by_clause::do_ostream(std::ostream& out) const -> void {
  out << "by (";
  bool first = true;
  for (const std::string& s : tag_names_) {
    if (!std::exchange(first, false))
      out << ", ";
    out << maybe_quote_identifier(s);
  }
  out << ")";
  if (keep_common_)
    out << " keep_common";
}

auto by_clause::reduce_tag_(const tags& t) const -> std::optional<tags> {
  std::vector<std::tuple<std::string, metric_value>> filtered;
  filtered.reserve(tag_names_.size());

  for (const std::string& name : tag_names_) {
    std::optional<metric_value> v = t[name];
    if (!v.has_value()) return {};

    filtered.emplace_back(name, std::move(v).value());
  }

  return tags(std::make_move_iterator(filtered.begin()),
              std::make_move_iterator(filtered.end()));
}

auto by_clause::map_(std::unordered_map<tags, metric_value>& x) const
->  mapping {
  mapping result;

  for (auto xval = x.begin(); xval != x.end(); ++xval) {
    std::optional<tags> reduced = reduce_tag_(xval->first);
    if (!reduced.has_value()) continue;

    result.emplace(std::move(reduced).value(), xval);
  }

  return result;
}

auto by_clause::select_tags_(const tags& key, const tags& x, const tags& y)
    const
->  tags {
  if (!keep_common_) return key;

  std::vector<std::tuple<std::string, metric_value>> result;
  std::set_intersection(x.get_map().begin(), x.get_map().end(),
                        y.get_map().begin(), y.get_map().end(),
                        std::back_inserter(result),
                        [](tags::map_type::const_reference x,
                           tags::map_type::const_reference y) {
                          if (x.first != y.first) return x.first < y.first;
                          return metric_value::before(x.second, y.second);
                        });
  return tags(std::make_move_iterator(result.begin()),
              std::make_move_iterator(result.end()));
}


}} /* namespace monsoon::match_clauses */
