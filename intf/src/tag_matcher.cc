#include <monsoon/tag_matcher.h>
#include <ostream>
#include <sstream>
#include <unordered_set>
#include <monsoon/overload.h>
#include <monsoon/config_support.h>

namespace monsoon {
namespace {


bool has_defined_equality(const metric_value& x, const metric_value& y) {
  return equal(x, y).as_bool().has_value();
}

bool has_defined_compare(const metric_value& x, const metric_value& y) {
  return less(x, y).as_bool().has_value();
}

bool has_overlap(const tag_matcher::comparison_match& x,
                 const tag_matcher::comparison_match& y) {
  // Make elements of x and y easier to access.
  const tag_matcher::comparison x_op = std::get<0>(x);
  const tag_matcher::comparison y_op = std::get<0>(y);
  const metric_value& x_val = std::get<1>(x);
  const metric_value& y_val = std::get<1>(y);

  // Declare case labels for ease of use.
  constexpr tag_matcher::comparison eq = tag_matcher::comparison::eq;
  constexpr tag_matcher::comparison ne = tag_matcher::comparison::ne;
  constexpr tag_matcher::comparison lt = tag_matcher::comparison::lt;
  constexpr tag_matcher::comparison gt = tag_matcher::comparison::gt;
  constexpr tag_matcher::comparison le = tag_matcher::comparison::le;
  constexpr tag_matcher::comparison ge = tag_matcher::comparison::ge;

  metric_value mv;
  switch(x_op) {
    case eq:
      switch(y_op) {
        case eq:
          mv = equal(x_val, y_val);
          break;
        case ne:
          mv = unequal(x_val, y_val);
          break;
        case lt:
          mv = less(x_val, y_val);
          break;
        case gt:
          mv = greater(x_val, y_val);
          break;
        case le:
          mv = less_equal(x_val, y_val);
          break;
        case ge:
          mv = greater_equal(x_val, y_val);
          break;
      }
      break;
    case ne:
      switch(y_op) {
        case ne:
          mv = metric_value(has_defined_equality(x_val, y_val));
          break;
        case lt: [[fallthrough]];
        case gt: [[fallthrough]];
        case le: [[fallthrough]];
        case ge:
          mv = metric_value(has_defined_compare(x_val, y_val));
          break;
        case eq:
          mv = unequal(x_val, y_val);
          break;
      }
      break;
    case lt:
      switch(y_op) {
        case ne: [[fallthrough]];
        case lt: [[fallthrough]];
        case le:
          mv = metric_value(has_defined_compare(x_val, y_val));
          break;
        case gt: [[fallthrough]];
        case ge:
          mv = greater(x_val, y_val);
          break;
        case eq:
          mv = less(y_val, x_val);
          break;
      }
      break;
    case gt:
      switch(y_op) {
        case ne: [[fallthrough]];
        case gt: [[fallthrough]];
        case ge:
          mv = metric_value(has_defined_compare(x_val, y_val));
          break;
        case lt: [[fallthrough]];
        case le:
          mv = less(x_val, y_val);
          break;
        case eq:
          mv = greater(y_val, x_val);
          break;
      }
      break;
    case le:
      switch(y_op) {
        case ne: [[fallthrough]];
        case lt: [[fallthrough]];
        case le:
          mv = metric_value(has_defined_compare(x_val, y_val));
          break;
        case gt:
          mv = greater(x_val, y_val);
          break;
        case ge:
          mv = greater_equal(x_val, y_val);
          break;
        case eq:
          mv = less(y_val, x_val);
          break;
      }
      break;
    case ge:
      switch(y_op) {
        case ne: [[fallthrough]];
        case gt: [[fallthrough]];
        case ge:
          mv = metric_value(has_defined_compare(x_val, y_val));
          break;
        case lt:
          mv = less(x_val, y_val);
          break;
        case le:
          mv = less_equal(x_val, y_val);
          break;
        case eq:
          mv = greater(y_val, x_val);
          break;
      }
      break;
  }
  return mv.as_bool().value_or(false);
}


} /* namespace monsoon::<unnamed> */


bool tag_matcher::operator()(const tags& t) const {
  for (const auto& m : matcher_) {
    const bool cmp = std::visit(
        overload(
            [&t, &m](const presence_match&) {
              return t[m.first].has_value();
            },
            [&t, &m](const absence_match&) {
              return !t[m.first].has_value();
            },
            [&t, &m](const comparison_match& cmp) {
              auto opt_mv = t[m.first];
              if (!opt_mv) return false;

              metric_value mv;
              switch (std::get<0>(cmp)) {
                case eq:
                  mv = equal(opt_mv.value(), std::get<1>(cmp));
                  break;
                case ne:
                  mv = unequal(opt_mv.value(), std::get<1>(cmp));
                  break;
                case lt:
                  mv = less(opt_mv.value(), std::get<1>(cmp));
                  break;
                case gt:
                  mv = greater(opt_mv.value(), std::get<1>(cmp));
                  break;
                case le:
                  mv = less_equal(opt_mv.value(), std::get<1>(cmp));
                  break;
                case ge:
                  mv = greater_equal(opt_mv.value(), std::get<1>(cmp));
                  break;
              }
              return mv.as_bool().value_or(false);
            }),
        m.second);

    if (!cmp) return false;
  }
  return true;
}

void tag_matcher::check_comparison(std::string&& tagname, comparison cmp,
    metric_value&& tagvalue) {
  matcher_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(std::move(tagname)),
      std::forward_as_tuple(
          std::in_place_type<comparison_match>,
          std::move(cmp),
          std::move(tagvalue)));
}

void tag_matcher::check_comparison(std::string_view tagname, comparison cmp,
    const metric_value& tagvalue) {
  matcher_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(tagname),
      std::forward_as_tuple(
          std::in_place_type<comparison_match>,
          std::move(cmp),
          tagvalue));
}

void tag_matcher::check_presence(std::string&& tagname) {
  matcher_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(std::move(tagname)),
      std::forward_as_tuple(std::in_place_type<presence_match>));
}

void tag_matcher::check_presence(std::string_view tagname) {
  matcher_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(tagname),
      std::forward_as_tuple(std::in_place_type<presence_match>));
}

void tag_matcher::check_absence(std::string&& tagname) {
  matcher_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(std::move(tagname)),
      std::forward_as_tuple(std::in_place_type<absence_match>));
}

void tag_matcher::check_absence(std::string_view tagname) {
  matcher_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(tagname),
      std::forward_as_tuple(std::in_place_type<absence_match>));
}


bool has_overlap(const tag_matcher& x, const tag_matcher& y) {
  using absence_match = tag_matcher::absence_match;
  using presence_match = tag_matcher::presence_match;
  using comparison_match = tag_matcher::comparison_match;

  // Compare ranges using intersecting keys.
  // Note: cartesian product of ranges with the same key.
  for (const auto& x_entry : x) {
    tag_matcher::matcher_map::const_iterator y_range_b, y_range_e;
    std::tie(y_range_b, y_range_e) = y.matcher_.equal_range(x_entry.first);

    // Check if this x_entry is compatible with all y_entries with matching keys.
    const bool ranges_overlap = std::all_of(
        y_range_b, y_range_e,
        [&x_entry](const auto& y_entry) {
          return std::visit(
              overload(
                  // Both absent?
                  []([[maybe_unused]] const absence_match& x, [[maybe_unused]] const absence_match& y) {
                    return true;
                  },
                  // Fail: one absent, one present.
                  []([[maybe_unused]] const absence_match& x, [[maybe_unused]] const auto& y) {
                    return false;
                  },
                  []([[maybe_unused]] const auto& x, [[maybe_unused]] const absence_match& y) {
                    return false;
                  },
                  // Presence matches with any presence.
                  []([[maybe_unused]] const presence_match& x, [[maybe_unused]] const presence_match& y) {
                    return true;
                  },
                  []([[maybe_unused]] const presence_match& x, [[maybe_unused]] const comparison_match& y) {
                    return true;
                  },
                  []([[maybe_unused]] const comparison_match& x, [[maybe_unused]] const presence_match& y) {
                    return true;
                  },
                  // Comparison match.
                  [](const comparison_match& x, const comparison_match& y) {
                    return has_overlap(x, y);
                  }),
              x_entry.second, y_entry.second);
        });
    if (!ranges_overlap) return false;
  }
  return true;
}


auto operator<<(std::ostream& out, const tag_matcher& tm) -> std::ostream& {
  using namespace std::placeholders;
  using comparison = tag_matcher::comparison;
  using comparison_match = tag_matcher::comparison_match;
  using presence_match = tag_matcher::presence_match;
  using absence_match = tag_matcher::absence_match;

  bool first = true;
  for (const auto& v : tm) {
    if (!std::exchange(first, false)) out << ", ";
    std::visit(
        std::bind(
            overload(
                [&out](std::string_view qname, const presence_match&) {
                  out << qname;
                },
                [&out](std::string_view qname, const absence_match&) {
                  out << "!" << qname;
                },
                [&out](std::string_view qname, const comparison_match& cmp) {
                  std::string_view cmp_str;
                  switch (std::get<0>(cmp)) {
                    case tag_matcher::eq:
                      cmp_str = "=";
                      break;
                    case tag_matcher::ne:
                      cmp_str = "!=";
                      break;
                    case tag_matcher::lt:
                      cmp_str = "<";
                      break;
                    case tag_matcher::gt:
                      cmp_str = ">";
                      break;
                    case tag_matcher::le:
                      cmp_str = "<=";
                      break;
                    case tag_matcher::ge:
                      cmp_str = ">=";
                      break;
                  }
                  out << qname << cmp_str << std::get<1>(cmp);
                }
            ),
            maybe_quote_identifier(v.first), _1),
        v.second);
  }
  return out;
}

std::string to_string(const tag_matcher& tm) {
  return (std::ostringstream() << tm).str();
}


} /* namespace monsoon */
