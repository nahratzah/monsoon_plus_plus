#include <monsoon/grammar/expression/ast.h>
#include <monsoon/expressions/constant.h>
#include <monsoon/expressions/operators.h>
#include <monsoon/expressions/selector.h>
#include <monsoon/overload.h>

namespace monsoon {
namespace grammar {
namespace ast {


namespace {


struct resolve_expr {
  using result_type = expression_ptr;

  template<typename T>
  expression_ptr operator()(const T& v) const {
    return v;
  }

  template<typename T>
  expression_ptr operator()(const x3::forward_ast<T>& v) const {
    return (*this)(v.get());
  }
};


} /* namespace monsoon::grammar::ast::<unnamed> */


constant_expr::operator expression_ptr() const {
  return expressions::constant(v);
}

path_matcher_expr::operator expressions::path_matcher() const {
  expressions::path_matcher result;
  for (const auto& i : *this) {
    std::visit(
        overload(
            [&result](const expressions::path_matcher::wildcard&) {
              result.push_back_wildcard();
            },
            [&result](const expressions::path_matcher::double_wildcard&) {
              result.push_back_double_wildcard();
            },
            [&result](std::string_view s) {
              result.push_back_literal(s);
            }),
        i);
  }
  return result;
}

tag_matcher_expr::operator expressions::tag_matcher() const {
  using std::bind;
  using namespace std::placeholders;

  expressions::tag_matcher result;
  for (const auto& i : *this) {
    i.apply_visitor(
        bind<void>(
            [&result](const auto& v) {
              std::apply(
                  overload(
                      [&result](
                          std::string_view tagname,
                          const expressions::tag_matcher::presence_match&) {
                        result.check_presence(tagname);
                      },
                      [&result](
                          std::string_view tagname,
                          const expressions::tag_matcher::absence_match&) {
                        result.check_absence(tagname);
                      },
                      [&result](
                          std::string_view tagname,
                          const expressions::tag_matcher::comparison& cmp,
                          const auto& value) {
                        result.check_comparison(tagname, cmp, value);
                      }),
                  v);
            },
            _1));
  }
  return result;
}

selector_expr::operator expression_ptr() const {
  return expressions::selector(
      groupname,
      tagset,
      metricname);
}

primary_expr::operator expression_ptr() const {
  return this->apply_visitor(resolve_expr());
}

unary_expr::operator expression_ptr() const {
  return this->apply_visitor(resolve_expr());
}

logical_negate_expr::operator expression_ptr() const {
  return expressions::logical_not(v);
}

numeric_negate_expr::operator expression_ptr() const {
  return expressions::numeric_negate(v);
}

logical_and_expr::operator expression_ptr() const {
  expression_ptr result = at(0);
  for (auto i = ++begin(), e = end(); i != e; ++i)
    result = expressions::logical_and(std::move(result), *i);
  return result;
}

logical_or_expr::operator expression_ptr() const {
  expression_ptr result = at(0);
  for (auto i = ++begin(), e = end(); i != e; ++i)
    result = expressions::logical_or(std::move(result), *i);
  return result;
}


expression_ptr apply(equality_enum e, expression_ptr x, expression_ptr y) {
  switch (e) {
    case equality_enum::eq:
      return expressions::cmp_eq(std::move(x), std::move(y));
    case equality_enum::ne:
      return expressions::cmp_ne(std::move(x), std::move(y));
  }
}

expression_ptr apply(compare_enum e, expression_ptr x, expression_ptr y) {
  switch (e) {
    case compare_enum::ge:
      return expressions::cmp_ge(std::move(x), std::move(y));
    case compare_enum::le:
      return expressions::cmp_le(std::move(x), std::move(y));
    case compare_enum::gt:
      return expressions::cmp_gt(std::move(x), std::move(y));
    case compare_enum::lt:
      return expressions::cmp_lt(std::move(x), std::move(y));
  }
}

expression_ptr apply(shift_enum e, expression_ptr x, expression_ptr y) {
  switch (e) {
    case shift_enum::left:
      return expressions::numeric_shift_left(std::move(x), std::move(y));
    case shift_enum::right:
      return expressions::numeric_shift_right(std::move(x), std::move(y));
  }
}

expression_ptr apply(addsub_enum e, expression_ptr x, expression_ptr y) {
  switch (e) {
    case addsub_enum::add:
      return expressions::numeric_add(std::move(x), std::move(y));
    case addsub_enum::sub:
      return expressions::numeric_subtract(std::move(x), std::move(y));
  }
}

expression_ptr apply(muldiv_enum e, expression_ptr x, expression_ptr y) {
  switch (e) {
    case muldiv_enum::mul:
      return expressions::numeric_multiply(std::move(x), std::move(y));
    case muldiv_enum::div:
      return expressions::numeric_divide(std::move(x), std::move(y));
    case muldiv_enum::mod:
      return expressions::numeric_modulo(std::move(x), std::move(y));
  }
}


}}} /* namespace monsoon::grammar::ast */
