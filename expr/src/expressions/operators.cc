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
 private:
  class application;

 public:
  unop_t(Fn&&, std::string_view, expression_ptr&&);
  ~unop_t() noexcept override;

  void operator()(acceptor<emit_type>&, const metric_source&,
      const time_range&, time_point::duration) const override;

 private:
  void do_ostream(std::ostream&) const override;

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

  void operator()(acceptor<emit_type>&, const metric_source&,
      const time_range&, time_point::duration) const override;

 private:
  void do_ostream(std::ostream&) const override;

  expression_ptr x_, y_;
  Fn fn_;
  std::string_view sign_;
};


template<typename Fn>
class monsoon_expr_local_ unop_t<Fn>::application final
: public acceptor<emit_type>
{
 public:
  application(acceptor<emit_type>&, const Fn&) noexcept;
  ~application() noexcept override;

  void accept_speculative(time_point, const emit_type&) override;
  void accept(time_point, vector_type) override;

 private:
  acceptor<emit_type>& accept_fn_;
  const Fn& fn_;
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
void unop_t<Fn>::operator()(
    acceptor<emit_type>& accept_fn,
    const metric_source& src,
    const time_range& tr, time_point::duration slack) const {
  auto app = application(accept_fn, fn_);
  (*nested_)(app, src, tr, std::move(slack));
}

template<typename Fn>
void unop_t<Fn>::do_ostream(std::ostream& out) const {
  out << sign_ << *nested_;
}


template<typename Fn>
unop_t<Fn>::application::application(acceptor<emit_type>& accept_fn,
    const Fn& fn) noexcept
: accept_fn_(accept_fn),
  fn_(fn)
{}

template<typename Fn>
unop_t<Fn>::application::~application() noexcept {}

template<typename Fn>
void unop_t<Fn>::application::accept_speculative(time_point tp,
    const emit_type& emt) {
  std::visit(
      overload(
          [tp, this](const metric_value& mv) {
            accept_fn_.accept_speculative(std::move(tp), fn_(std::move(mv)));
          },
          [tp, this](std::tuple<tags, metric_value> mv_tpl) {
            std::get<1>(mv_tpl) = fn_(std::get<1>(std::move(mv_tpl)));
            accept_fn_.accept_speculative(std::move(tp), std::move(mv_tpl));
          }),
      emt);
}

template<typename Fn>
void unop_t<Fn>::application::accept(time_point tp, vector_type v) {
  std::for_each(v.begin(), v.end(),
      [this](std::tuple<emit_type>& outer_tpl) {
        auto& emt = std::get<0>(outer_tpl);
        std::visit(
            overload(
                [this](metric_value& mv) {
                  mv = fn_(std::move(mv));
                },
                [this](std::tuple<tags, metric_value>& mv_tpl) {
                  std::get<1>(mv_tpl) = fn_(std::get<1>(std::move(mv_tpl)));
                }),
            emt);
      });
  accept_fn_.accept(std::move(tp), std::move(v));
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
