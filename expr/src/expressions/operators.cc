#include <monsoon/expressions/operators.h>
#include <monsoon/metric_value.h>
#include <type_traits>
#include <string_view>
#include <ostream>
#include <utility>
#include <algorithm>
#include <functional>
#include "../overload.h"

namespace monsoon {
namespace expressions {


template<typename Fn>
class monsoon_expr_local_ unop_t final
: public expression
{
 public:
  unop_t(Fn&&, std::string_view, expression_ptr&&);
  ~unop_t() noexcept override;

  auto operator()(const metric_source&,
      const time_range&, time_point::duration) const
      -> objpipe::reader<emit_type> override;

 private:
  void do_ostream(std::ostream&) const override;

  static auto apply_(emit_type&, const Fn&) -> emit_type&;

  expression_ptr nested_;
  Fn fn_;
  std::string_view sign_;
};


template<typename Fn>
class monsoon_expr_local_ binop_t final
: public expression
{
 private:
  class application;

 public:
  binop_t(Fn&&, std::string_view, expression_ptr&&, expression_ptr&&);
  ~binop_t() noexcept override;

  auto operator()(const metric_source&,
      const time_range&, time_point::duration) const
      -> objpipe::reader<emit_type> override;

 private:
  void do_ostream(std::ostream&) const override;

  expression_ptr x_, y_;
  Fn fn_;
  std::string_view sign_;
};


template<typename Fn>
unop_t<Fn>::unop_t(Fn&& fn, std::string_view sign, expression_ptr&& nested)
: fn_(std::move(fn)),
  sign_(std::move(sign)),
  nested_(std::move(nested))
{
  if (nested_ == nullptr) throw std::invalid_argument("null expression_ptr");
}

template<typename Fn>
unop_t<Fn>::~unop_t() noexcept {}

template<typename Fn>
auto unop_t<Fn>::operator()(
    const metric_source& src,
    const time_range& tr, time_point::duration slack) const
-> objpipe::reader<emit_type> {
  using namespace std::placeholders;

  return std::invoke(*nested_, src, tr, std::move(slack))
      .transform_copy(std::bind(&unop_t::apply_, _1, fn_));
}

template<typename Fn>
void unop_t<Fn>::do_ostream(std::ostream& out) const {
  out << sign_ << *nested_;
}

template<typename Fn>
auto unop_t<Fn>::apply_(emit_type& emt, const Fn& fn) -> emit_type& {
  if (std::get<1>(emt).index() == 0u) {
    auto& speculative = std::get<0>(std::get<1>(emt));

    std::visit(
        overload(
            [&fn](metric_value& v) {
              v = std::invoke(fn, v);
            },
            [&fn](std::tuple<tags, metric_value>& tpl) {
              std::get<1>(tpl) = std::invoke(fn, std::get<1>(tpl));
            }),
        speculative);
  } else {
    auto& factual = std::get<1>(std::get<1>(emt));

    std::visit(
        overload(
            [&fn](metric_value& v) {
              v = std::invoke(fn, v);
            },
            [&fn](std::unordered_map<tags, metric_value>& map) {
              for (auto& elem : map) {
                elem.second = std::invoke(fn, elem.second);
              }
            }),
        factual);
  }

  return emt;
}


template<typename Fn>
binop_t<Fn>::binop_t(Fn&& fn, std::string_view sign,
    expression_ptr&& x, expression_ptr&& y)
: fn_(std::move(fn)),
  sign_(std::move(sign)),
  x_(std::move(x)),
  y_(std::move(y))
{
  if (x_ == nullptr) throw std::invalid_argument("null expression_ptr x");
  if (y_ == nullptr) throw std::invalid_argument("null expression_ptr y");
}

template<typename Fn>
binop_t<Fn>::~binop_t() noexcept {}

template<typename Fn>
void binop_t<Fn>::do_ostream(std::ostream& out) const {
  out << *x_ << sign_ << *y_;
}


template<typename Fn>
monsoon_expr_local_
auto unop(Fn&& fn, const std::string_view& sign, expression_ptr&& nested)
-> expression_ptr {
  return expression::make_ptr<unop_t<std::decay_t<Fn>>>(
      std::forward<Fn>(fn), sign, std::move(nested));
}

template<typename Fn>
monsoon_expr_local_
auto binop(Fn&& fn, const std::string_view& sign,
    expression_ptr&& x, expression_ptr&& y)
-> expression_ptr {
  return expression::make_ptr<binop_t<std::decay_t<Fn>>>(
      std::forward<Fn>(fn), sign, std::move(x), std::move(y));
}


expression_ptr logical_not(expression_ptr ptr) {
  static constexpr std::string_view sign{"!"};
  return unop(
      [](const metric_value& x) { return !x; },
      sign,
      std::move(ptr));
}

expression_ptr logical_and(expression_ptr x, expression_ptr y) {
  static constexpr std::string_view sign{" && "};
  return binop(
      [](const metric_value& x, const metric_value& y) { return x && y; },
      sign,
      std::move(x), std::move(y));
}

expression_ptr logical_or(expression_ptr x, expression_ptr y) {
  static constexpr std::string_view sign{" || "};
  return binop(
      [](const metric_value& x, const metric_value& y) { return x || y; },
      sign,
      std::move(x), std::move(y));
}

expression_ptr cmp_eq(expression_ptr x, expression_ptr y) {
  static constexpr std::string_view sign{" = "};
  return binop(
      [](const metric_value& x, const metric_value& y) {
        return equal(x, y);
      },
      sign,
      std::move(x), std::move(y));
}

expression_ptr cmp_ne(expression_ptr x, expression_ptr y) {
  static constexpr std::string_view sign{" != "};
  return binop(
      [](const metric_value& x, const metric_value& y) {
        return unequal(x, y);
      },
      sign,
      std::move(x), std::move(y));
}

expression_ptr cmp_lt(expression_ptr x, expression_ptr y) {
  static constexpr std::string_view sign{" < "};
  return binop(
      [](const metric_value& x, const metric_value& y) {
        return less(x, y);
      },
      sign,
      std::move(x), std::move(y));
}

expression_ptr cmp_gt(expression_ptr x, expression_ptr y) {
  static constexpr std::string_view sign{" > "};
  return binop(
      [](const metric_value& x, const metric_value& y) {
        return greater(x, y);
      },
      sign,
      std::move(x), std::move(y));
}

expression_ptr cmp_le(expression_ptr x, expression_ptr y) {
  static constexpr std::string_view sign{" <= "};
  return binop(
      [](const metric_value& x, const metric_value& y) {
        return less_equal(x, y);
      },
      sign,
      std::move(x), std::move(y));
}

expression_ptr cmp_ge(expression_ptr x, expression_ptr y) {
  static constexpr std::string_view sign{" >= "};
  return binop(
      [](const metric_value& x, const metric_value& y) {
        return greater_equal(x, y);
      },
      sign,
      std::move(x), std::move(y));
}

expression_ptr numeric_negate(expression_ptr ptr) {
  static constexpr std::string_view sign{"-"};
  return unop(
      [](const metric_value& x) { return -x; },
      sign,
      std::move(ptr));
}

expression_ptr numeric_add(expression_ptr x, expression_ptr y) {
  static constexpr std::string_view sign{" + "};
  return binop(
      [](const metric_value& x, const metric_value& y) { return x + y; },
      sign,
      std::move(x), std::move(y));
}

expression_ptr numeric_subtract(expression_ptr x, expression_ptr y) {
  static constexpr std::string_view sign{" - "};
  return binop(
      [](const metric_value& x, const metric_value& y) { return x - y; },
      sign,
      std::move(x), std::move(y));
}

expression_ptr numeric_multiply(expression_ptr x, expression_ptr y) {
  static constexpr std::string_view sign{" * "};
  return binop(
      [](const metric_value& x, const metric_value& y) { return x * y; },
      sign,
      std::move(x), std::move(y));
}

expression_ptr numeric_divide(expression_ptr x, expression_ptr y) {
  static constexpr std::string_view sign{" / "};
  return binop(
      [](const metric_value& x, const metric_value& y) { return x / y; },
      sign,
      std::move(x), std::move(y));
}

expression_ptr numeric_modulo(expression_ptr x, expression_ptr y) {
  static constexpr std::string_view sign{" % "};
  return binop(
      [](const metric_value& x, const metric_value& y) { return x % y; },
      sign,
      std::move(x), std::move(y));
}


}} /* namespace monsoon::expressions */
