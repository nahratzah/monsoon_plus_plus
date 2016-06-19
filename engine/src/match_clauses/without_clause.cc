#include <monsoon/match_clauses/without_clause.h>
#include <monsoon/config_support.h>
#include <algorithm>
#include <iterator>
#include <ostream>

namespace monsoon {
namespace match_clauses {


without_clause::~without_clause() noexcept {}

auto without_clause::apply(
    std::unordered_map<tags, metric_value> x,
    std::unordered_map<tags, metric_value> y,
    std::function<metric_value(metric_value, metric_value)> fn) const
->  std::unordered_map<tags, metric_value> {
  std::unordered_map<tags, metric_value> result;
  const mapping x_mapping = map_(x);
  const mapping y_mapping = map_(y);

  auto x_iter = x_mapping.begin();
  auto y_iter = y_mapping.begin();
  while (x_iter != x_mapping.end() && y_iter != y_mapping.end()) {
    if (x_iter->first < y_iter->first) {
      ++x_iter;
    } else if (y_iter->first < x_iter->first) {
      ++y_iter;
    } else {
      result.emplace(select_tags_(x_iter->first,
                                  x_iter->second->first,
                                  y_iter->second->first),
                     fn(std::move(x_iter->second->second),
                        std::move(y_iter->second->second)));
      ++x_iter;
      ++y_iter;
    }
  }

  return result;
}

auto without_clause::do_ostream(std::ostream& out) const -> void {
  out << "without (";
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

auto without_clause::reduce_tag_(const tags& t) const -> tags {
  auto filtered = t.get_map();

  for (const std::string& name : tag_names_)
    filtered.erase(name);
  return tags(filtered.begin(), filtered.end());
}

auto without_clause::map_(std::unordered_map<tags, metric_value>& x) const
->  mapping {
  mapping result;

  for (auto xval = x.begin(); xval != x.end(); ++xval)
    result.emplace(reduce_tag_(xval->first), xval);
  return result;
}

auto without_clause::select_tags_(const tags& key,
                                  const tags& x, const tags& y) const
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
