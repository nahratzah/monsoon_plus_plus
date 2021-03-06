#include <monsoon/expressions/operators.h>
#include <monsoon/expressions/merger.h>
#include <monsoon/metric_value.h>
#include <monsoon/interpolate.h>
#include <monsoon/overload.h>
#include <type_traits>
#include <string_view>
#include <ostream>
#include <utility>
#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <deque>
#include <iterator>
#include <mutex>

namespace monsoon {
namespace expressions {


class monsoon_expr_local_ unop_t final
: public expression
{
 public:
  using functor = metric_value (*)(const metric_value&);

  unop_t(functor, std::string_view, expression_ptr&&, precedence);
  ~unop_t() noexcept override;

  auto operator()(const metric_source&,
      const time_range&, time_point::duration,
      const std::shared_ptr<const match_clause>&) const
      -> std::variant<scalar_objpipe, vector_objpipe> override;

  bool is_scalar() const noexcept override;
  bool is_vector() const noexcept override;

 private:
  void do_ostream(std::ostream&) const override;

  static auto apply_scalar_(scalar_emit_type&, functor) -> void;
  static auto apply_vector_(vector_emit_type&, functor) -> void;

  functor fn_;
  expression_ptr nested_;
  std::string_view sign_;
};


class monsoon_expr_local_ binop_t final
: public expression
{
 private:
  class application;

 public:
  using functor = metric_value(*)(const metric_value&, const metric_value&);

  binop_t(functor, std::string_view, expression_ptr&&, expression_ptr&&,
      precedence,
      std::shared_ptr<const match_clause> mc);
  ~binop_t() noexcept override;

  auto operator()(const metric_source&,
      const time_range&, time_point::duration,
      const std::shared_ptr<const match_clause>&) const
      -> std::variant<scalar_objpipe, vector_objpipe> override;

  bool is_scalar() const noexcept override;
  bool is_vector() const noexcept override;

 private:
  void do_ostream(std::ostream&) const override;

  functor fn_;
  expression_ptr x_, y_;
  std::string_view sign_;
  std::shared_ptr<const match_clause> mc_;
};


unop_t::unop_t(functor fn, std::string_view sign, expression_ptr&& nested,
    precedence level)
: expression(level),
  fn_(std::move(fn)),
  nested_(std::move(nested)),
  sign_(std::move(sign))
{
  if (nested_ == nullptr) throw std::invalid_argument("null expression_ptr");
}

unop_t::~unop_t() noexcept {}

auto unop_t::operator()(
    const metric_source& src,
    const time_range& tr, time_point::duration slack,
    const std::shared_ptr<const match_clause>& out_mc) const
-> std::variant<scalar_objpipe, vector_objpipe> {
  using namespace std::placeholders;

  return std::visit(
      overload(
          [this](scalar_objpipe&& s)
          -> std::variant<scalar_objpipe, vector_objpipe> {
            return std::move(s)
                .peek(std::bind(&unop_t::apply_scalar_, _1, fn_));
          },
          [this](vector_objpipe&& s)
          -> std::variant<scalar_objpipe, vector_objpipe> {
            return std::move(s)
                .peek(std::bind(&unop_t::apply_vector_, _1, fn_));
          }),
      std::invoke(*nested_, src, tr, std::move(slack), out_mc));
}

bool unop_t::is_scalar() const noexcept {
  return nested_->is_scalar();
}

bool unop_t::is_vector() const noexcept {
  return nested_->is_vector();
}

void unop_t::do_ostream(std::ostream& out) const {
  bool need_braces = (nested_->level < level);
  out << sign_ << (need_braces ? "(" : "") << *nested_ << (need_braces ? ")" : "");
}

auto unop_t::apply_scalar_(scalar_emit_type& emt, functor fn)
-> void {
  std::visit(
      [fn](metric_value& v) {
        v = std::invoke(fn, std::move(v));
      },
      emt.data);
}

auto unop_t::apply_vector_(vector_emit_type& emt, functor fn)
-> void {
  std::visit(
      overload(
          [fn](speculative_vector& v) {
            std::get<1>(v) = std::invoke(fn, std::move(std::get<1>(v)));
          },
          [fn](factual_vector& map) {
            for (auto& elem : map)
              elem.second = std::invoke(fn, std::move(elem.second));
          }),
      emt.data);
}


binop_t::binop_t(functor fn, std::string_view sign,
    expression_ptr&& x, expression_ptr&& y,
    precedence level,
    std::shared_ptr<const match_clause> mc)
: expression(level),
  fn_(std::move(fn)),
  x_(std::move(x)),
  y_(std::move(y)),
  sign_(std::move(sign)),
  mc_(std::move(mc))
{
  if (x_ == nullptr) throw std::invalid_argument("null expression_ptr x");
  if (y_ == nullptr) throw std::invalid_argument("null expression_ptr y");
}

binop_t::~binop_t() noexcept {}

bool binop_t::is_scalar() const noexcept {
  return x_->is_scalar() && y_->is_scalar();
}

bool binop_t::is_vector() const noexcept {
  return x_->is_vector() || y_->is_vector();
}

auto binop_t::operator()(const metric_source& src,
    const time_range& tr, time_point::duration slack,
    const std::shared_ptr<const match_clause>& out_mc) const
-> std::variant<scalar_objpipe, vector_objpipe> {
  return std::visit(
      overload(
          [this, &out_mc, slack](scalar_objpipe&& x, scalar_objpipe&& y)
          -> std::variant<scalar_objpipe, vector_objpipe> {
            return make_merger(fn_, slack, std::move(x), std::move(y));
          },
          [this, &out_mc, slack](auto&& x, auto&& y)
          -> std::variant<scalar_objpipe, vector_objpipe> {
            return make_merger(fn_, mc_, out_mc, slack, std::forward<decltype(x)>(x), std::forward<decltype(y)>(y));
          }),
      (*x_)(src, tr, slack, mc_),
      (*y_)(src, tr, slack, mc_));
}

void binop_t::do_ostream(std::ostream& out) const {
  bool x_need_braces = (x_->level < level);
  bool y_need_braces = (y_->level <= level);
  out
      << (x_need_braces ? "(" : "")
      << *x_
      << (x_need_braces ? ")" : "")
      << sign_
      << (y_need_braces ? "(" : "")
      << *y_
      << (y_need_braces ? ")" : "");
}


inline auto unop(unop_t::functor fn,
    const std::string_view& sign,
    expression_ptr&& nested,
    expression::precedence level)
-> expression_ptr {
  return expression::make_ptr<unop_t>(
      fn, sign, std::move(nested), level);
}

inline auto binop(binop_t::functor fn,
    const std::string_view& sign,
    expression_ptr&& x, expression_ptr&& y,
    expression::precedence level,
    std::shared_ptr<const match_clause> mc)
-> expression_ptr {
  return expression::make_ptr<binop_t>(
      fn, sign, std::move(x), std::move(y), level, std::move(mc));
}


expression_ptr logical_not(expression_ptr ptr) {
  static constexpr std::string_view sign{"!"};
  return unop(
      [](const metric_value& x) { return !x; },
      sign,
      std::move(ptr),
      expression::precedence_negate);
}

expression_ptr logical_and(expression_ptr x, expression_ptr y,
    std::shared_ptr<const match_clause> mc) {
  static constexpr std::string_view sign{" && "};
  return binop(
      [](const metric_value& x, const metric_value& y) { return x && y; },
      sign,
      std::move(x), std::move(y),
      expression::precedence_logical_and,
      std::move(mc));
}

expression_ptr logical_or(expression_ptr x, expression_ptr y,
    std::shared_ptr<const match_clause> mc) {
  static constexpr std::string_view sign{" || "};
  return binop(
      [](const metric_value& x, const metric_value& y) { return x || y; },
      sign,
      std::move(x), std::move(y),
      expression::precedence_logical_or,
      std::move(mc));
}

expression_ptr cmp_eq(expression_ptr x, expression_ptr y,
    std::shared_ptr<const match_clause> mc) {
  static constexpr std::string_view sign{" = "};
  return binop(
      [](const metric_value& x, const metric_value& y) {
        return equal(x, y);
      },
      sign,
      std::move(x), std::move(y),
      expression::precedence_equality,
      std::move(mc));
}

expression_ptr cmp_ne(expression_ptr x, expression_ptr y,
    std::shared_ptr<const match_clause> mc) {
  static constexpr std::string_view sign{" != "};
  return binop(
      [](const metric_value& x, const metric_value& y) {
        return unequal(x, y);
      },
      sign,
      std::move(x), std::move(y),
      expression::precedence_equality,
      std::move(mc));
}

expression_ptr cmp_lt(expression_ptr x, expression_ptr y,
    std::shared_ptr<const match_clause> mc) {
  static constexpr std::string_view sign{" < "};
  return binop(
      [](const metric_value& x, const metric_value& y) {
        return less(x, y);
      },
      sign,
      std::move(x), std::move(y),
      expression::precedence_compare,
      std::move(mc));
}

expression_ptr cmp_gt(expression_ptr x, expression_ptr y,
    std::shared_ptr<const match_clause> mc) {
  static constexpr std::string_view sign{" > "};
  return binop(
      [](const metric_value& x, const metric_value& y) {
        return greater(x, y);
      },
      sign,
      std::move(x), std::move(y),
      expression::precedence_compare,
      std::move(mc));
}

expression_ptr cmp_le(expression_ptr x, expression_ptr y,
    std::shared_ptr<const match_clause> mc) {
  static constexpr std::string_view sign{" <= "};
  return binop(
      [](const metric_value& x, const metric_value& y) {
        return less_equal(x, y);
      },
      sign,
      std::move(x), std::move(y),
      expression::precedence_compare,
      std::move(mc));
}

expression_ptr cmp_ge(expression_ptr x, expression_ptr y,
    std::shared_ptr<const match_clause> mc) {
  static constexpr std::string_view sign{" >= "};
  return binop(
      [](const metric_value& x, const metric_value& y) {
        return greater_equal(x, y);
      },
      sign,
      std::move(x), std::move(y),
      expression::precedence_compare,
      std::move(mc));
}

expression_ptr numeric_negate(expression_ptr ptr) {
  static constexpr std::string_view sign{"-"};
  return unop(
      [](const metric_value& x) { return -x; },
      sign,
      std::move(ptr),
      expression::precedence_negate);
}

expression_ptr numeric_add(expression_ptr x, expression_ptr y,
    std::shared_ptr<const match_clause> mc) {
  static constexpr std::string_view sign{" + "};
  return binop(
      [](const metric_value& x, const metric_value& y) { return x + y; },
      sign,
      std::move(x), std::move(y),
      expression::precedence_add_subtract,
      std::move(mc));
}

expression_ptr numeric_subtract(expression_ptr x, expression_ptr y,
    std::shared_ptr<const match_clause> mc) {
  static constexpr std::string_view sign{" - "};
  return binop(
      [](const metric_value& x, const metric_value& y) { return x - y; },
      sign,
      std::move(x), std::move(y),
      expression::precedence_add_subtract,
      std::move(mc));
}

expression_ptr numeric_multiply(expression_ptr x, expression_ptr y,
    std::shared_ptr<const match_clause> mc) {
  static constexpr std::string_view sign{" * "};
  return binop(
      [](const metric_value& x, const metric_value& y) { return x * y; },
      sign,
      std::move(x), std::move(y),
      expression::precedence_multiply_divide,
      std::move(mc));
}

expression_ptr numeric_divide(expression_ptr x, expression_ptr y,
    std::shared_ptr<const match_clause> mc) {
  static constexpr std::string_view sign{" / "};
  return binop(
      [](const metric_value& x, const metric_value& y) { return x / y; },
      sign,
      std::move(x), std::move(y),
      expression::precedence_multiply_divide,
      std::move(mc));
}

expression_ptr numeric_modulo(expression_ptr x, expression_ptr y,
    std::shared_ptr<const match_clause> mc) {
  static constexpr std::string_view sign{" % "};
  return binop(
      [](const metric_value& x, const metric_value& y) { return x % y; },
      sign,
      std::move(x), std::move(y),
      expression::precedence_multiply_divide,
      std::move(mc));
}

expression_ptr numeric_shift_left(expression_ptr x, expression_ptr y,
    std::shared_ptr<const match_clause> mc) {
  static constexpr std::string_view sign{" << "};
  return binop(
      [](const metric_value& x, const metric_value& y) { return x << y; },
      sign,
      std::move(x), std::move(y),
      expression::precedence_shift,
      std::move(mc));
}

expression_ptr numeric_shift_right(expression_ptr x, expression_ptr y,
    std::shared_ptr<const match_clause> mc) {
  static constexpr std::string_view sign{" >> "};
  return binop(
      [](const metric_value& x, const metric_value& y) { return x >> y; },
      sign,
      std::move(x), std::move(y),
      expression::precedence_shift,
      std::move(mc));
}


}} /* namespace monsoon::expressions */
