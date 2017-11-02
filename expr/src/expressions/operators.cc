#include <monsoon/expressions/operators.h>
#include <monsoon/metric_value.h>
#include <monsoon/interpolate.h>
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
      -> std::variant<scalar_objpipe, vector_objpipe> override;

  bool is_scalar() const noexcept override;
  bool is_vector() const noexcept override;

 private:
  void do_ostream(std::ostream&) const override;

  static auto apply_scalar_(scalar_emit_type&, const Fn&) -> scalar_emit_type&;
  static auto apply_vector_(vector_emit_type&, const Fn&) -> vector_emit_type&;

  Fn fn_;
  expression_ptr nested_;
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
      -> std::variant<scalar_objpipe, vector_objpipe> override;

  bool is_scalar() const noexcept override;
  bool is_vector() const noexcept override;

 private:
  void do_ostream(std::ostream&) const override;

  Fn fn_;
  expression_ptr x_, y_;
  std::string_view sign_;
};


class monsoon_expr_local_ scalar_accumulator {
 private:
  using speculative_map = std::map<time_point, metric_value>;
  using factual_list = std::deque<std::pair<time_point, metric_value>>;

 public:
  auto operator[](time_point tp) const
      -> std::optional<std::tuple<metric_value, bool>>;
  auto factual_until() const noexcept -> std::optional<time_point>;
  void advance_factual(time_point) noexcept;
  void add(expression::scalar_emit_type&& v);

 private:
  void add_speculative_(time_point tp, metric_value&& v);
  void add_factual_(time_point tp, metric_value&& v);

  speculative_map speculative_;
  factual_list factual_;
};


class monsoon_expr_local_ vector_accumulator {
 private:
  struct speculative_cmp {
    using is_transparent = std::true_type;

    bool operator()(
        std::tuple<time_point, const tags&> x,
        std::tuple<time_point, const tags&> y) const noexcept {
      return x < y;
    }

    bool operator()(
        std::tuple<time_point, const tags&> x,
        const time_point& y) const noexcept {
      return std::get<0>(x) < y;
    }

    bool operator()(
        const time_point& x,
        std::tuple<time_point, const tags&> y) const noexcept {
      return x < std::get<0>(y);
    }
  };

  using speculative_map = std::map<
      std::tuple<time_point, tags>,
      metric_value,
      speculative_cmp>;

  struct speculative_index_cmp {
   private:
    using internal_type = std::tuple<const tags&, time_point>;

   public:
    using is_transparent = std::true_type;

    template<typename X, typename Y>
    bool operator()(const X& x, const Y& y) const noexcept {
      return intern_(x) < intern_(y);
    }

   private:
    static auto intern_(const speculative_map::const_iterator i) noexcept
    -> internal_type {
      return intern_(i->first);
    }

    static auto intern_(const internal_type& i) noexcept
    -> const internal_type& {
      return i;
    }

    static auto intern_(std::tuple<time_point, const tags&> i)
    -> internal_type {
      return std::tie(std::get<1>(i), std::get<0>(i));
    }
  };

  using speculative_index = std::set<
      speculative_map::const_iterator,
      speculative_index_cmp>;
  using factual_list =
      std::deque<std::pair<time_point, expression::factual_vector>>;

 public:
  struct tp_proxy {
   public:
    tp_proxy(const vector_accumulator& self, time_point tp) noexcept
    : self_(self),
      tp_(tp)
    {}

    auto operator[](const tags& tag_set) const
        -> std::optional<std::tuple<metric_value, bool>> {
      return self_.interpolate_(tp_, tag_set);
    }

    bool has_value() const noexcept {
      return self_.factual_until() >= tp_;
    }

    auto value() const
        -> std::variant<
            expression::factual_vector,
            const expression::factual_vector*> {
      return self_.interpolate_(tp_);
    }

   private:
    const vector_accumulator& self_;
    time_point tp_;
  };

  auto operator[](time_point tp) const -> tp_proxy {
    return tp_proxy(*this, tp);
  }

  auto factual_until() const noexcept -> std::optional<time_point>;
  void advance_factual(time_point) noexcept;
  void add(expression::vector_emit_type&& v);

 private:
  void add_speculative_(time_point tp, expression::speculative_vector&& v);
  void add_factual_(time_point tp, expression::factual_vector&& v);
  auto interpolate_(time_point tp, const tags& tag_set) const
      -> std::optional<std::tuple<metric_value, bool>>;
  auto interpolate_(time_point tp) const
      -> std::variant<
          expression::factual_vector,
          const expression::factual_vector*>;

  speculative_map speculative_;
  factual_list factual_;
  speculative_index speculative_index_;
};


template<typename Fn>
unop_t<Fn>::unop_t(Fn&& fn, std::string_view sign, expression_ptr&& nested)
: fn_(std::move(fn)),
  nested_(std::move(nested)),
  sign_(std::move(sign))
{
  if (nested_ == nullptr) throw std::invalid_argument("null expression_ptr");
}

template<typename Fn>
unop_t<Fn>::~unop_t() noexcept {}

template<typename Fn>
auto unop_t<Fn>::operator()(
    const metric_source& src,
    const time_range& tr, time_point::duration slack) const
-> std::variant<scalar_objpipe, vector_objpipe> {
  using namespace std::placeholders;

  return std::visit(
      overload(
          [this](scalar_objpipe&& s)
          -> std::variant<scalar_objpipe, vector_objpipe> {
            return std::move(s)
                .transform_copy(std::bind(&unop_t::apply_scalar_, _1, fn_));
          },
          [this](vector_objpipe&& s)
          -> std::variant<scalar_objpipe, vector_objpipe> {
            return std::move(s)
                .transform_copy(std::bind(&unop_t::apply_vector_, _1, fn_));
          }),
      std::invoke(*nested_, src, tr, std::move(slack)));
}

template<typename Fn>
bool unop_t<Fn>::is_scalar() const noexcept {
  return nested_->is_scalar();
}

template<typename Fn>
bool unop_t<Fn>::is_vector() const noexcept {
  return nested_->is_scalar();
}

template<typename Fn>
void unop_t<Fn>::do_ostream(std::ostream& out) const {
  out << sign_ << *nested_;
}

template<typename Fn>
auto unop_t<Fn>::apply_scalar_(scalar_emit_type& emt, const Fn& fn)
-> scalar_emit_type& {
  std::visit(
      [&fn](metric_value& v) {
        v = std::invoke(fn, std::move(v));
      },
      emt.data);
  return emt;
}

template<typename Fn>
auto unop_t<Fn>::apply_vector_(vector_emit_type& emt, const Fn& fn)
-> vector_emit_type& {
  std::visit(
      overload(
          [&fn](speculative_vector& v) {
            std::get<1>(v) = std::invoke(fn, std::move(std::get<1>(v)));
          },
          [&fn](factual_vector& map) {
            for (auto& elem : map)
              elem.second = std::invoke(fn, std::move(elem.second));
          }),
      emt.data);
  return emt;
}


template<typename Fn>
binop_t<Fn>::binop_t(Fn&& fn, std::string_view sign,
    expression_ptr&& x, expression_ptr&& y)
: fn_(std::move(fn)),
  x_(std::move(x)),
  y_(std::move(y)),
  sign_(std::move(sign))
{
  if (x_ == nullptr) throw std::invalid_argument("null expression_ptr x");
  if (y_ == nullptr) throw std::invalid_argument("null expression_ptr y");
}

template<typename Fn>
binop_t<Fn>::~binop_t() noexcept {}

template<typename Fn>
bool binop_t<Fn>::is_scalar() const noexcept {
  return x_->is_scalar() && y_->is_scalar();
}

template<typename Fn>
bool binop_t<Fn>::is_vector() const noexcept {
  return x_->is_vector() || y_->is_vector();
}

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


auto scalar_accumulator::operator[](time_point tp) const
-> std::optional<std::tuple<metric_value, bool>> {
  using result_tpl = std::tuple<metric_value, bool>;

  if (factual_.empty() || tp > factual_.back().first) {
    const auto at_after = speculative_.lower_bound(tp);
    if (at_after == speculative_.end()) return {};
    if (at_after->first == tp)
      return std::forward_as_tuple(at_after->second, false);

    if (at_after == speculative_.begin()) {
      if (factual_.empty()) return {};
      auto interpolated = interpolate(tp, factual_.back(), *at_after);
      if (!interpolated.has_value()) return {};
      return std::forward_as_tuple(std::move(interpolated).value(), false);
    }

    const auto before = std::prev(at_after);
    auto interpolated = interpolate(tp, *before, *at_after);
    if (!interpolated.has_value()) return {};
    return std::forward_as_tuple(std::move(interpolated).value(), false);
  } else {
    const auto at_after = std::lower_bound(factual_.begin(), factual_.end(),
        tp,
        [](const factual_list::value_type& x,
            const time_point& y) {
          return x.first < y;
        });
    if (at_after == factual_.end()) return {};
    if (at_after->first == tp)
      return std::forward_as_tuple(at_after->second, true);
    if (at_after == factual_.begin()) return {};

    const auto before = std::prev(at_after);
    auto interpolated = interpolate(tp, *before, *at_after);
    if (!interpolated.has_value()) return {};
    return std::forward_as_tuple(std::move(interpolated).value(), true);
  }
}

auto scalar_accumulator::factual_until() const noexcept
-> std::optional<time_point> {
  if (factual_.empty()) return {};
  return factual_.back().first;
}

void scalar_accumulator::advance_factual(time_point tp) noexcept {
  if (factual_.empty()) return;

  factual_list::const_iterator erase_end;
  if (factual_.back().first == tp) {
    // Common case: erase everything but most recent entry.
    erase_end = std::prev(factual_.cend());
  } else {
    // Point erase_end at largest element at/before tp.
    erase_end = std::prev(
        std::upper_bound(std::next(factual_.cbegin()), factual_.cend(),
            tp,
            [](const time_point& x,
                const factual_list::value_type& y) {
              return x < y.first;
            }));
  }

  factual_.erase(factual_.begin(), erase_end);
}

void scalar_accumulator::add(expression::scalar_emit_type&& v) {
  switch (v.data.index()) {
    default:
      assert(false);
      break;
    case 0u:
      add_speculative_(std::move(v.tp), std::get<0>(std::move(v.data)));
      break;
    case 1u:
      add_factual_(std::move(v.tp), std::get<1>(std::move(v.data)));
      break;
  }
}

void scalar_accumulator::add_speculative_(time_point tp, metric_value&& v) {
  assert(factual_.empty() || factual_.back().first < tp);
  speculative_.emplace(std::move(tp), std::move(v));
}

void scalar_accumulator::add_factual_(time_point tp, metric_value&& v) {
  assert(factual_.empty() || factual_.back().first < tp);
  factual_.emplace_back(tp, std::move(v));

  // Erase all cached speculative entries at/before tp.
  speculative_.erase(speculative_.begin(), speculative_.upper_bound(tp));
}


auto vector_accumulator::factual_until() const noexcept
-> std::optional<time_point> {
  if (factual_.empty()) return {};
  return factual_.back().first;
}

void vector_accumulator::advance_factual(time_point tp) noexcept {
  if (factual_.empty()) return;

  factual_list::const_iterator erase_end;
  if (factual_.back().first == tp) {
    // Common case: erase everything but most recent entry.
    erase_end = std::prev(factual_.cend());
  } else {
    // Point erase_end at largest element at/before tp.
    erase_end = std::prev(
        std::upper_bound(std::next(factual_.cbegin()), factual_.cend(),
            tp,
            [](const time_point& x,
                const factual_list::value_type& y) {
              return x < y.first;
            }));
  }

  factual_.erase(factual_.begin(), erase_end);
}

void vector_accumulator::add(expression::vector_emit_type&& v) {
  switch (v.data.index()) {
    default:
      assert(false);
      break;
    case 0u:
      add_speculative_(std::move(v.tp), std::get<0>(std::move(v.data)));
      break;
    case 1u:
      add_factual_(std::move(v.tp), std::get<1>(std::move(v.data)));
      break;
  }
}

void vector_accumulator::add_speculative_(time_point tp,
    expression::speculative_vector&& v) {
  assert(factual_.empty() || factual_.back().first < tp);

  bool inserted;
  speculative_map::iterator ins_pos;
  std::tie(ins_pos, inserted) = speculative_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(std::move(tp), std::get<0>(std::move(v))),
      std::forward_as_tuple(std::get<1>(std::move(v))));

  if (inserted) {
    try {
      bool idx_inserted;
      std::tie(std::ignore, idx_inserted) = speculative_index_.insert(ins_pos);
      assert(idx_inserted);
    } catch (...) {
      speculative_.erase(ins_pos);
      throw;
    }
  }
}

void vector_accumulator::add_factual_(time_point tp,
    expression::factual_vector&& v) {
  assert(factual_.empty() || factual_.back().first < tp);
  factual_.emplace_back(tp, std::move(v));

  // Erase all cached speculative entries at/before tp.
  const auto erase_end = speculative_.upper_bound(tp);
  for (auto i = speculative_.cbegin(); i != erase_end; ++i)
    speculative_index_.erase(i);
  speculative_.erase(speculative_.begin(), speculative_.upper_bound(tp));
}

auto vector_accumulator::interpolate_(time_point tp, const tags& tag_set) const
-> std::optional<std::tuple<metric_value, bool>> {
  if (factual_.empty() || factual_.back().first < tp) {
    const speculative_index::const_iterator idx_at_after =
        speculative_index_.lower_bound(std::tie(tp, tag_set));
    if (idx_at_after == speculative_index_.end()) return {};
    const auto at_after = *idx_at_after;
    if (std::get<1>(at_after->first) != tag_set) return {};
    if (std::get<0>(at_after->first) == tp)
      return std::forward_as_tuple(at_after->second, false);

    speculative_index::const_iterator idx_before;
    if (idx_at_after != speculative_index_.begin())
      idx_before = std::prev(idx_at_after);
    if (idx_at_after == speculative_index_.begin()
        || std::get<1>((*idx_before)->first) != tag_set) {
      if (factual_.empty()) return {};
      const auto& factual_back = factual_.back();
      auto before = factual_back.second.find(tag_set);
      if (before == factual_back.second.end()) return {};

      auto interpolated = interpolate(tp,
          std::tie(factual_back.first, before->second),
          std::tie(std::get<0>(at_after->first), at_after->second));
      if (!interpolated.has_value()) return {};
      return std::forward_as_tuple(std::move(interpolated).value(), false);
    }

    auto before = *idx_before;
    auto interpolated = interpolate(tp,
        std::tie(std::get<0>(before->first), before->second),
        std::tie(std::get<0>(at_after->first), at_after->second));
    if (!interpolated.has_value()) return {};
    return std::forward_as_tuple(std::move(interpolated).value(), false);
  } else {
    const auto at_after = std::lower_bound(factual_.begin(), factual_.end(),
        tp,
        [](const factual_list::value_type& x,
            const time_point& y) {
          return x.first < y;
        });
    if (at_after == factual_.end()) return {};

    if (at_after->first == tp) {
      const auto tag_match = at_after->second.find(tag_set);
      if (tag_match == at_after->second.end()) return {};
      return std::forward_as_tuple(tag_match->second, true);
    }

    if (at_after == factual_.begin()) return {};
    const auto after_match = at_after->second.find(tag_set);
    if (after_match == at_after->second.end()) return {};
    const auto before = std::prev(at_after);
    const auto before_match = before->second.find(tag_set);
    if (before_match == before->second.end()) return {};

    auto interpolated = interpolate(tp,
        std::tie(before->first, before_match->second),
        std::tie(at_after->first, after_match->second));
    if (!interpolated.has_value()) return {};
    return std::forward_as_tuple(std::move(interpolated).value(), true);
  }
}

auto vector_accumulator::interpolate_(time_point tp) const
-> std::variant<
    expression::factual_vector,
    const expression::factual_vector*> {
  assert(!factual_.empty());
  assert(tp <= factual_.back().first);

  const auto at_after = std::lower_bound(factual_.begin(), factual_.end(),
      tp,
      [](const factual_list::value_type& x,
          const time_point& y) {
        return x.first < y;
      });
  assert(at_after != factual_.end());

  // Return reference (which is a pointer) to non-interpolated map.
  if (at_after->first == tp)
    return &at_after->second;

  // Yield empty map if the first factual map is after the tp.
  assert(at_after->first > tp);
  if (at_after == factual_.begin()) return expression::factual_vector();
  const auto before = std::prev(at_after);

  // Create interpolated map.
  expression::factual_vector interpolated;
  interpolated.reserve(std::min(
          before->second.size(),
          at_after->second.size()));
  // Convenience constants.
  const time_point before_tp = before->first;
  const time_point after_tp = at_after->first;
  const expression::factual_vector& after_map = at_after->second;
  // Iterate the before map, interpolating each value with the at_after map.
  // Result is stored in 'interpolated' variable.
  std::for_each(before->second.begin(), before->second.end(),
      [&interpolated, tp, before_tp, after_tp, &after_map](const auto& tagged_metric_before) {
        auto tagged_metric_after = after_map.find(tagged_metric_before.first);
        if (tagged_metric_after != after_map.end()) {
          auto interpolated_value = interpolate(tp,
              std::tie(before_tp, tagged_metric_before.second),
              std::tie(after_tp, tagged_metric_after->second));
          if (interpolated_value.has_value()) {
            interpolated.emplace(
                tagged_metric_before.first,
                std::move(interpolated_value).value());
          }
        }
      });
  return interpolated;
}


template<typename ObjPipe>
class merger_managed {
 public:
  ///\brief The accumulator type managed by this.
  using accumulator_type = std::conditional_t<
      std::is_same_v<expression::scalar_objpipe, ObjPipe>,
      scalar_accumulator,
      std::enable_if_t<
          std::is_same_v<expression::vector_objpipe, ObjPipe>,
          vector_accumulator>>;

  explicit merger_managed(ObjPipe&& input)
  : input_(std::move(input))
  {
#if 0
    input_.on_event(
        std::bind(this, &managed_scalar::on_notify_),
        std::bind(this, &managed_scalar::on_close_));
#endif
  }

  /**
   * \brief Load values until the next factual emition.
   *
   * \return true if a factual emition was loaded, false otherwise.
   * A \p true return value also means the objpipe may have more data
   * ready and should be pulled from without waiting for a notification.
   *
   * \param callback \parblock
   *   Callback invoked with speculative data.
   *
   *   The callback signature for scalars should be similar to
   *   \code
   *   void(const expression::scalar_emit_type&)
   *   \endcode
   *
   *   The callback signature for vectors should be similar to
   *   \code
   *   void(const expression::vector_emit_type&)
   *   \endcode
   * \endparblock
   * \note \p callback may not access the accumulator.
   */
  template<typename Callback>
  bool load_until_next_factual(Callback callback) {
    if (!input_) return false;

    for (auto val = input_.try_pull();
        val.has_value();
        val = input_.try_pull()) {
      const bool is_factual = (val->data.index() == 1u);
      const auto& c_val = *val;
      std::invoke(callback, c_val);

      accumulator.add(*std::move(val));
      if (is_factual) return true;
    }
    return false;
  }

  /**
   * \brief Load all values.
   *
   * \return true if a factual emition was loaded, false otherwise.
   *
   * \param callback \parblock
   *   Callback invoked with speculative data.
   *
   *   The callback signature for scalars should be similar to
   *   \code
   *   void(const expression::scalar_emit_type&)
   *   \endcode
   *
   *   The callback signature for vectors should be similar to
   *   \code
   *   void(const expression::vector_emit_type&)
   *   \endcode
   * \endparblock
   * \note \p callback may not access the accumulator.
   */
  template<typename Callback>
  bool load_all(Callback callback) {
    if (!input_) return false;

    bool factual_encountered = false;
    // Use for_each, so data can be released after the call.
    std::move(input_).for_each(
        [&](auto&& val) {
          const bool is_factual = (val.data.index() == 1u);
          const auto& c_val = val;
          std::invoke(callback, c_val);

          accumulator.add(std::move(val));
          if (is_factual) factual_encountered = true;
        });
    return factual_encountered;
  }

  accumulator_type accumulator;

 private:
  ObjPipe input_;
};


struct monsoon_expr_local_ unpack_scalar_ {
  explicit unpack_scalar_(const expression::scalar_emit_type& v)
  : tp(v.tp),
    speculative(v.data.index() == 0u)
  {}

  auto operator()(const scalar_accumulator& m) -> std::optional<metric_value> {
    std::optional<std::tuple<metric_value, bool>> opt_mv = m[tp];
    if (!opt_mv.has_value()) return {};
    speculative |= !std::get<1>(*opt_mv);
    return std::get<0>(*std::move(opt_mv));
  }

  // XXX
#if 0
  auto operator()(const vector_accumulator& m) -> std::optional<...> {
    const auto proxy = m[tp];
    if (proxy.is_speculative()) speculative = true;
    return m[tp].value();
  }
#endif

  time_point tp;
  bool speculative;
};

struct monsoon_expr_local_ unpack_vector_ {
  using tag_set_t = std::variant<
      const tags*,
      std::pair<
          expression::factual_vector::const_iterator,
          expression::factual_vector::const_iterator>>;

  explicit unpack_vector_(const expression::vector_emit_type& v)
  : tp(v.tp),
    tag_set(std::visit(
          overload(
              [](const expression::speculative_vector& mv) -> tag_set_t {
                return &std::get<0>(mv);
              },
              [](const expression::factual_vector& mv) -> tag_set_t {
                return std::make_pair(mv.cbegin(), mv.cend());
              }),
          v.data)),
    speculative(v.data.index() == 0u)
  {}

  auto operator()(const scalar_accumulator& m) -> std::optional<metric_value> {
    std::optional<std::tuple<metric_value, bool>> opt_mv = m[tp];
    if (!opt_mv.has_value()) return {};
    speculative |= !std::get<1>(*opt_mv);
    return std::get<0>(*std::move(opt_mv));
  }

  // XXX
#if 0
  auto operator()(const vector_accumulator& m) -> std::optional<...> {
    using result_type = std::optional<...>;

    const auto proxy = m[tp];
    return std::visit(
        overload(
            [&proxy, this](const tags* t) -> result_type {
              std::optional<std::tuple<metric_value, bool>> opt_mv = proxy[*t];
              if (!opt_mv) return {};
              speculative |= (std::get<1>(*opt_mv));
              return std::get<0>(*opt_mv);
            },
            [&proxy, this](const std::pair<expression::factual_vector::const_iterator, expression::factual_vector::const_iterator>& iters) -> result_type {
              if (proxy.is_speculative()) speculative = true;
              return m[tp].value();
            }),
        tag_set);
  }
#endif

  time_point tp;
  tag_set_t tag_set;
  bool speculative;
};

monsoon_expr_local_
auto make_unpack_(const expression::scalar_emit_type& v)
-> unpack_scalar_ {
  return unpack_scalar_(v);
}

monsoon_expr_local_
auto make_unpack_(const expression::vector_emit_type& v)
-> unpack_vector_ {
  return unpack_vector_(v);
}

template<std::size_t CbIdx>
struct merger_invocation {
  template<typename Fn, typename... Managed, typename Value>
  void operator()(time_point tp, Fn& fn, const std::tuple<Managed...>& managed,
      const Value& v) {
    static_assert(sizeof...(Managed) > CbIdx, "Index out of range of managed objpipes.");
    invoke_(tp, fn, managed, v, std::index_sequence_for<Managed...>());
  }

 private:
  template<typename Fn, typename... Managed, typename Value,
      std::size_t... Indices>
  void invoke_(time_point tp, Fn& fn, const std::tuple<Managed...>& managed,
      const Value& v,
      std::index_sequence<Indices...> indices) {
    const auto& unpack = make_unpack_(v);
    unpack_invoke_(
        fn,
        tp,
        indices,
        this->template apply_unpack_<Indices>(
            unpack,
            this->template choose_value_<Indices>(
                std::get<Indices>(managed),
                v))...);
  }

  template<typename Fn, typename... Values, std::size_t... Indices>
  void unpack_invoke_(Fn& fn, time_point tp,
      std::index_sequence<Indices...>,
      Values&... values) {
    std::invoke(
        fn,
        tp,
        values...); // XXX
  }

  template<std::size_t Idx, typename Unpack, typename Value>
  auto apply_unpack_(const Unpack& unpack, const Value& value)
  -> std::conditional_t<
      Idx == CbIdx,
      const Value&,
      decltype(std::declval<const Unpack&>()(std::declval<const Value&>()))> {
    if constexpr(Idx == CbIdx)
      return value;
    else
      return unpack(value);
  }

  template<std::size_t Idx, typename Managed, typename Value>
  constexpr auto choose_value_(Managed& managed, const Value& value)
  -> std::conditional_t<Idx == CbIdx, const Value&, Managed&> {
    if constexpr(Idx == CbIdx) {
      return value;
    } else {
      return managed;
    }
  }

  ///\deprecated Unused.
  static constexpr bool all_of_() noexcept {
    return true;
  }

  ///\deprecated Unused.
  template<typename... Tail>
  static constexpr bool all_of_(bool head, Tail... tail) noexcept {
    return head && all_of_(tail...);
  }
};


template<typename Fn, typename... ObjPipes>
class merger {
 public:
  merger(Fn&& fn, ObjPipes&&... inputs)
  : managed_(std::move(inputs)...),
    fn_(std::move(fn))
  {}

 private:
  template<std::size_t CbIdx>
  void load_until_next_factual_() {
    using namespace std::placeholders;

    std::get<CbIdx>(managed_)
        .load_until_next_factual(
            std::bind(merger_invocation<CbIdx>(),
                _1,
                [this](auto&&... args) {
                  // XXX
                },
                std::ref(managed_),
                _2));
  }

  std::tuple<merger_managed<ObjPipes>...> managed_;
  Fn fn_;
};


}} /* namespace monsoon::expressions */
