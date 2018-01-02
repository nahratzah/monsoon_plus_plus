#include <monsoon/grammar/expression/ast.h>
#include <monsoon/expressions/constant.h>
#include <monsoon/expressions/operators.h>
#include <monsoon/expressions/selector.h>

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


auto by_clause_expr::build() const
-> std::shared_ptr<const match_clause> {
  return (keep.has_value()
      ? std::make_shared<by_match_clause>(names.begin(), names.end())
      : std::make_shared<by_match_clause>(names.begin(), names.end(), *keep));
}

auto without_clause_expr::build() const
-> std::shared_ptr<const match_clause> {
  return std::make_shared<without_match_clause>(names.begin(), names.end());
}

auto default_clause_expr::build() const
-> std::shared_ptr<const match_clause> {
  return std::make_shared<default_match_clause>();
}

auto match_clause_expr::build() const
-> std::shared_ptr<const match_clause> {
  using std::bind;
  using namespace std::placeholders;

  return this->apply_visitor(
      bind<std::shared_ptr<const match_clause>>(
          [](const auto& m) {
            return m.build();
          },
          _1));
}


expression_ptr apply(equality_enum e, std::shared_ptr<const match_clause> m,
    expression_ptr x, expression_ptr y) {
  switch (e) {
    case equality_enum::eq:
      return expressions::cmp_eq(std::move(x), std::move(y), std::move(m));
    case equality_enum::ne:
      return expressions::cmp_ne(std::move(x), std::move(y), std::move(m));
  }
}

expression_ptr apply(compare_enum e, std::shared_ptr<const match_clause> m,
    expression_ptr x, expression_ptr y) {
  switch (e) {
    case compare_enum::ge:
      return expressions::cmp_ge(std::move(x), std::move(y), std::move(m));
    case compare_enum::le:
      return expressions::cmp_le(std::move(x), std::move(y), std::move(m));
    case compare_enum::gt:
      return expressions::cmp_gt(std::move(x), std::move(y), std::move(m));
    case compare_enum::lt:
      return expressions::cmp_lt(std::move(x), std::move(y), std::move(m));
  }
}

expression_ptr apply(shift_enum e, std::shared_ptr<const match_clause> m,
    expression_ptr x, expression_ptr y) {
  switch (e) {
    case shift_enum::left:
      return expressions::numeric_shift_left(std::move(x), std::move(y),
          std::move(m));
    case shift_enum::right:
      return expressions::numeric_shift_right(std::move(x), std::move(y),
          std::move(m));
  }
}

expression_ptr apply(addsub_enum e, std::shared_ptr<const match_clause> m,
    expression_ptr x, expression_ptr y) {
  switch (e) {
    case addsub_enum::add:
      return expressions::numeric_add(std::move(x), std::move(y),
          std::move(m));
    case addsub_enum::sub:
      return expressions::numeric_subtract(std::move(x), std::move(y),
          std::move(m));
  }
}

expression_ptr apply(muldiv_enum e, std::shared_ptr<const match_clause> m,
    expression_ptr x, expression_ptr y) {
  switch (e) {
    case muldiv_enum::mul:
      return expressions::numeric_multiply(std::move(x), std::move(y),
          std::move(m));
    case muldiv_enum::div:
      return expressions::numeric_divide(std::move(x), std::move(y),
          std::move(m));
    case muldiv_enum::mod:
      return expressions::numeric_modulo(std::move(x), std::move(y),
          std::move(m));
  }
}

expression_ptr apply(logical_and_enum e, std::shared_ptr<const match_clause> m,
    expression_ptr x, expression_ptr y) {
  return expressions::logical_and(std::move(x), std::move(y), std::move(m));
}

expression_ptr apply(logical_or_enum e, std::shared_ptr<const match_clause> m,
    expression_ptr x, expression_ptr y) {
  return expressions::logical_or(std::move(x), std::move(y), std::move(m));
}


}}} /* namespace monsoon::grammar::ast */
