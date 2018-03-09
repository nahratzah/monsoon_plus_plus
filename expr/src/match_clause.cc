#include <monsoon/match_clause.h>
#include <algorithm>
#include <cassert>
#include <iterator>
#include <functional>
#include <monsoon/overload.h>

namespace monsoon {


match_clause::~match_clause() noexcept {}


by_match_clause::~by_match_clause() noexcept {}

bool by_match_clause::pass(const tags& x) const noexcept {
  return x.has_keys(tag_names_.begin(), tag_names_.end());
}

bool by_match_clause::less_cmp(const tags& x, const tags& y) const noexcept {
  for (const auto& name : tag_names_) {
    const std::optional<metric_value> x_val = x[name];
    const std::optional<metric_value> y_val = y[name];
    assert(x_val.has_value() && y_val.has_value());

    if (metric_value::before(*x_val, *y_val)) return true;
    if (metric_value::before(*y_val, *x_val)) return false;
  }
  return false;
}

tags by_match_clause::reduce(const tags& x, const tags& y) const {
  tags::map_type result;

  switch (keep_) {
    case match_clause_keep::selected:
      std::set_intersection(
          x.begin(), x.end(),
          tag_names_.begin(), tag_names_.end(),
          std::inserter(result, result.end()),
          overload(
              [](const std::pair<const tags::string_type, metric_value>& x, std::string_view y) {
                return x.first < y;
              },
              [](std::string_view x, const std::pair<const tags::string_type, metric_value>& y) {
                return x < y.first;
              }));
      break;
    case match_clause_keep::left:
      result = x.get_map();
      break;
    case match_clause_keep::right:
      result = y.get_map();
      break;
    case match_clause_keep::common:
      std::set_intersection(
          x.begin(), x.end(),
          y.begin(), y.end(),
          std::inserter(result, result.end()),
          [](const auto& x, const auto& y) {
            if (std::get<0>(x) != std::get<0>(y))
              return std::get<0>(x) < std::get<0>(y);
            return metric_value::before(x.second, y.second);
          });
      break;
  }

  return tags(std::move(result));
}

void by_match_clause::fixup_() noexcept {
  std::sort(tag_names_.begin(), tag_names_.end());
  tag_names_.erase(
      std::unique(tag_names_.begin(), tag_names_.end()),
      tag_names_.end());
  tag_names_.shrink_to_fit();
}

std::size_t by_match_clause::hash(const tags& x) const noexcept {
  std::size_t cumulative = 0;

  auto name = tag_names_.begin();
  for (const auto& [first, second] : x) {
    while (name != tag_names_.end() && std::string_view(*name) < first)
      ++name;

    if (name == tag_names_.end() || std::string_view(*name) != first) {
      using first_type =
          std::remove_const_t<std::remove_reference_t<decltype(first)>>;
      using second_type =
          std::remove_const_t<std::remove_reference_t<decltype(second)>>;
      const size_t h = 23u * std::hash<first_type>()(first) +
          std::hash<second_type>()(second);

      cumulative ^= h;
    }
  }
  return cumulative;
}

bool by_match_clause::eq_cmp(const tags& x, const tags& y) const noexcept {
  for (const auto& name : tag_names_) {
    const std::optional<metric_value> x_val = x[name];
    const std::optional<metric_value> y_val = y[name];
    assert(x_val.has_value() && y_val.has_value());

    if (metric_value::before(*x_val, *y_val)
        || metric_value::before(*y_val, *x_val)) return false;
  }
  return true;
}


without_match_clause::~without_match_clause() noexcept {}

bool without_match_clause::pass(const tags&) const noexcept {
  return true;
}

bool without_match_clause::less_cmp(const tags& x, const tags& y)
    const noexcept {
  auto x_i = x.begin(), y_i = y.begin();
  const auto x_end = x.end(), y_end = y.end();

  while (x_i != x_end && y_i != y_end) {
    std::string_view x_key = std::get<0>(*x_i);
    std::string_view y_key = std::get<0>(*y_i);
    const metric_value& x_val = std::get<1>(*x_i);
    const metric_value& y_val = std::get<1>(*y_i);

    if (x_key < y_key) {
      if (tag_names_.count(std::string(x_key.begin(), x_key.end())) == 0) return true;
      ++x_i;
    } else if (x_key > y_key) {
      if (tag_names_.count(std::string(y_key.begin(), y_key.end())) == 0) return false;
      ++y_i;
    } else {
      if (tag_names_.count(std::string(x_key.begin(), x_key.end())) == 0) {
        if (metric_value::before(x_val, y_val))
          return true;
        else if (metric_value::before(y_val, x_val))
          return false;
      }

      ++x_i;
      ++y_i;
    }
  }

  while (x_i != x_end) {
    std::string_view x_key = std::get<0>(*x_i);
    if (tag_names_.count(std::string(x_key.begin(), x_key.end())) == 0) return false;
    ++x_i;
  }

  while (y_i != y_end) {
    std::string_view y_key = std::get<0>(*y_i);
    if (tag_names_.count(std::string(y_key.begin(), y_key.end())) == 0) return true;
    ++y_i;
  }

  return false;
}

tags without_match_clause::reduce(const tags& x, const tags& y) const {
  tags::map_type result;
  std::copy_if(
      x.begin(), x.end(),
      std::inserter(result, result.end()),
      [this](const auto& kv) {
        return tag_names_.count(std::string(std::get<0>(kv).begin(), std::get<0>(kv).end())) == 0;
      });
  return tags(std::move(result));
}

std::size_t without_match_clause::hash(const tags& x) const noexcept {
  std::size_t cumulative = 0;

  for (const auto& [first, second] : x) {
    if (tag_names_.count(std::string(first.begin(), first.end())) == 0) {
      using first_type =
          std::remove_const_t<std::remove_reference_t<decltype(first)>>;
      using second_type =
          std::remove_const_t<std::remove_reference_t<decltype(second)>>;
      const size_t h = 23u * std::hash<first_type>()(first) +
          std::hash<second_type>()(second);

      cumulative ^= h;
    }
  }
  return cumulative;
}

bool without_match_clause::eq_cmp(const tags& x, const tags& y) const noexcept {
  auto x_i = x.begin(), y_i = y.begin();
  const auto x_end = x.end(), y_end = y.end();

  while (x_i != x_end && y_i != y_end) {
    std::string_view x_key = std::get<0>(*x_i);
    std::string_view y_key = std::get<0>(*y_i);

    if (tag_names_.count(std::string(x_key.begin(), x_key.end())) != 0) {
      ++x_i;
      continue;
    }
    if (tag_names_.count(std::string(y_key.begin(), y_key.end())) != 0) {
      ++y_i;
      continue;
    }

    if (*x_i != *y_i) return false;
  }

  while (x_i != x_end) {
    std::string_view x_key = std::get<0>(*x_i);
    if (tag_names_.count(std::string(x_key.begin(), x_key.end())) != 0)
      ++x_i;
    else
      return false;
  }

  while (y_i != y_end) {
    std::string_view y_key = std::get<0>(*y_i);
    if (tag_names_.count(std::string(y_key.begin(), y_key.end())) != 0)
      ++y_i;
    else
      return false;
  }

  return true;
}


default_match_clause::~default_match_clause() noexcept {}

bool default_match_clause::pass(const tags&) const noexcept {
  return true;
}

bool default_match_clause::less_cmp(const tags& x, const tags& y)
    const noexcept {
  return x < y;
}

tags default_match_clause::reduce(const tags& x, const tags& y) const {
  assert(x == y);
  return x;
}

std::size_t default_match_clause::hash(const tags& x) const noexcept {
  return std::hash<tags>()(x);
}

bool default_match_clause::eq_cmp(const tags& x, const tags& y)
    const noexcept {
  return std::equal_to<tags>()(x, y);
}


} /* namespace monsoon */
