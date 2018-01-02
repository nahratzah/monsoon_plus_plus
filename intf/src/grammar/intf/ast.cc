#include <monsoon/grammar/intf/ast.h>
#include <monsoon/overload.h>

namespace monsoon {
namespace grammar {
namespace ast {


value_expr::operator metric_value() const {
  return std::visit(
      [](const auto& v) { return metric_value(v); },
      *this);
}

histogram_range_expr::operator std::pair<histogram::range, std::double_t>() const {
  return std::make_pair(histogram::range(lo, hi), count);
}

histogram_expr::operator histogram() const {
  return histogram(begin(), end());
}

simple_path_lit_expr::operator metric_name() const {
  return metric_name(begin(), end());
}

simple_path_lit_expr::operator simple_group() const {
  return simple_group(begin(), end());
}

path_matcher_expr::operator path_matcher() const {
  path_matcher result;
  for (const auto& i : *this) {
    std::visit(
        overload(
            [&result](const path_matcher::wildcard&) {
              result.push_back_wildcard();
            },
            [&result](const path_matcher::double_wildcard&) {
              result.push_back_double_wildcard();
            },
            [&result](std::string_view s) {
              result.push_back_literal(s);
            }),
        i);
  }
  return result;
}

tag_matcher_expr::operator tag_matcher() const {
  using std::bind;
  using namespace std::placeholders;

  tag_matcher result;
  for (const auto& i : *this) {
    i.apply_visitor(
        bind<void>(
            [&result](const auto& v) {
              std::apply(
                  overload(
                      [&result](
                          std::string_view tagname,
                          const tag_matcher::presence_match&) {
                        result.check_presence(tagname);
                      },
                      [&result](
                          std::string_view tagname,
                          const tag_matcher::absence_match&) {
                        result.check_absence(tagname);
                      },
                      [&result](
                          std::string_view tagname,
                          const tag_matcher::comparison& cmp,
                          const value_expr& value) {
                        result.check_comparison(tagname, cmp, value);
                      }),
                  v);
            },
            _1));
  }
  return result;
}


}}} /* namespace monsoon::grammar::ast */
