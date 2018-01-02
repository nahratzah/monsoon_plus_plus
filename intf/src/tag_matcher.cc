#include <monsoon/tag_matcher.h>
#include <ostream>
#include <sstream>
#include <monsoon/overload.h>
#include <monsoon/config_support.h>

namespace monsoon {


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
