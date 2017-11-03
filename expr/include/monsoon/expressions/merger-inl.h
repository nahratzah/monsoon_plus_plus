#ifndef MONSOON_EXPRESSIONS_MERGER_INL_H
#define MONSOON_EXPRESSIONS_MERGER_INL_H

#include <cassert>
#include <monsoon/overload.h>

namespace monsoon {
namespace expressions {


inline bool vector_accumulator::speculative_cmp::operator()(
    std::tuple<time_point, const tags&> x,
    std::tuple<time_point, const tags&> y) const noexcept {
  return x < y;
}

inline bool vector_accumulator::speculative_cmp::operator()(
    std::tuple<time_point, const tags&> x,
    const time_point& y) const noexcept {
  return std::get<0>(x) < y;
}

inline bool vector_accumulator::speculative_cmp::operator()(
    const time_point& x,
    std::tuple<time_point, const tags&> y) const noexcept {
  return x < std::get<0>(y);
}


template<typename X, typename Y>
bool vector_accumulator::speculative_index_cmp::operator()(
    const X& x, const Y& y) const noexcept {
  return intern_(x) < intern_(y);
}

inline auto vector_accumulator::speculative_index_cmp::intern_(
    const speculative_map::const_iterator i) noexcept
-> internal_type {
  return intern_(i->first);
}

inline auto vector_accumulator::speculative_index_cmp::intern_(
    const internal_type& i) noexcept
-> const internal_type& {
  return i;
}

inline auto vector_accumulator::speculative_index_cmp::intern_(
    std::tuple<time_point, const tags&> i) noexcept
-> internal_type {
  return std::tie(std::get<1>(i), std::get<0>(i));
}


inline vector_accumulator::tp_proxy::tp_proxy(const vector_accumulator& self, time_point tp) noexcept
: self_(self),
  tp_(tp)
{}

inline auto vector_accumulator::tp_proxy::operator[](const tags& tag_set) const
-> std::optional<std::tuple<metric_value, bool>> {
  return self_.interpolate_(tp_, tag_set);
}

inline bool vector_accumulator::tp_proxy::is_speculative() const noexcept {
  return self_.factual_until() < tp_;
}

inline auto vector_accumulator::tp_proxy::value() const
-> std::variant<
    expression::factual_vector,
    std::reference_wrapper<const expression::factual_vector>> {
  if (is_speculative())
    return {}; // XXX create speculative interpolation
  else
    return self_.interpolate_(tp_);
}


inline auto vector_accumulator::operator[](time_point tp) const -> tp_proxy {
  return tp_proxy(*this, tp);
}


namespace {


template<typename ObjPipe>
merger_managed<ObjPipe>::merger_managed(ObjPipe&& input)
: input_(std::move(input))
{}

template<typename ObjPipe>
template<typename Callback>
bool merger_managed<ObjPipe>::load_until_next_factual(Callback callback) {
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

template<typename ObjPipe>
auto merger_managed<ObjPipe>::factual_until() const noexcept
-> std::optional<time_point> {
  return accumulator.factual_until();
}

template<typename ObjPipe>
bool merger_managed<ObjPipe>::is_pullable() const noexcept {
  return bool(input_);
}

template<typename ObjPipe>
void merger_managed<ObjPipe>::advance_factual(time_point tp) {
  accumulator.advance_factual(tp);
}


inline unpack_::unpack_(const expression::vector_emit_type& v)
: tp(v.tp),
  tag_set(std::visit(
        overload(
            [](const expression::speculative_vector& mv) -> tag_set_t {
              return &std::get<0>(mv);
            },
            [](const expression::factual_vector& mv) -> tag_set_t {
              return nullptr;
            }),
        v.data)),
  speculative(v.data.index() == 0u)
{}

inline unpack_::unpack_(const expression::scalar_emit_type& v)
: tp(v.tp),
  speculative(v.data.index() == 0u)
{}

inline auto unpack_::operator()(const scalar_accumulator& m)
-> std::optional<metric_value> {
  std::optional<std::tuple<metric_value, bool>> opt_mv = m[tp];
  if (!opt_mv.has_value()) return {};
  speculative |= !std::get<1>(*opt_mv);
  return std::get<0>(*std::move(opt_mv));
}

inline auto unpack_::operator()(const vector_accumulator& m)
-> std::optional<std::variant<
    std::tuple<tags, metric_value>,
    expression::factual_vector,
    std::reference_wrapper<const expression::factual_vector>>> {
  using variant_type = std::variant<
      std::tuple<tags, metric_value>,
      expression::factual_vector,
      std::reference_wrapper<const expression::factual_vector>>;

  const auto proxy = m[tp];
  speculative |= proxy.is_speculative();
  if (tag_set != nullptr) {
    std::optional<std::tuple<metric_value, bool>> opt_mv = proxy[*tag_set];
    if (!opt_mv) return {};
    assert(speculative || !std::get<1>(*opt_mv));
    return std::make_tuple(*tag_set, std::get<0>(*std::move(opt_mv)));
  } else {
    return visit(
        overload(
            [](expression::factual_vector&& v) -> variant_type {
              return std::move(v);
            },
            [](const expression::factual_vector& v) -> variant_type {
              return std::cref(v);
            }),
        m[tp].value());
  }
}


inline auto left_tag_combiner_::operator()(const tags& x, const tags&)
    const noexcept
-> const tags& {
  return x;
}

inline auto left_tag_combiner_::operator()(tags&& x, const tags&)
    const noexcept
-> tags&& {
  return std::move(x);
}


template<typename Fn, typename TagEqual, typename TagCombine>
recursive_apply<Fn, TagEqual, TagCombine>::recursive_apply(Fn fn,
    TagEqual equal, TagCombine combine)
: fn_(std::move(fn)),
  tag_equal_(std::move(equal)),
  tag_combine_(std::move(combine))
{}

template<typename Fn, typename TagEqual, typename TagCombine>
template<typename... Values>
void recursive_apply<Fn, TagEqual, TagCombine>::operator()(
    Values&&... values) {
  std::array<metric_value, sizeof...(Values)> args;
  recursive_apply_(
      untagged(),
      args,
      std::forward<Values>(values)...);
}

template<typename Fn, typename TagEqual, typename TagCombine>
template<std::size_t N>
void recursive_apply<Fn, TagEqual, TagCombine>::recursive_apply_(
    const untagged&,
    const std::array<metric_value, N>& values) {
  std::invoke(fn_, values);
}

template<typename Fn, typename TagEqual, typename TagCombine>
template<std::size_t N>
void recursive_apply<Fn, TagEqual, TagCombine>::recursive_apply_(
    const tags& tags,
    const std::array<metric_value, N>& values) {
  std::invoke(fn_, tags, values);
}

template<typename Fn, typename TagEqual, typename TagCombine>
template<typename Tags, std::size_t N, typename Head, typename... Tail>
void recursive_apply<Fn, TagEqual, TagCombine>::recursive_apply_(
    const Tags& tags,
    std::array<metric_value, N>& values,
    const std::optional<Head>& head,
    Tail&&... tail) {
  if (head.has_value())
    this->recursive_apply_(tags, values, *head, std::forward<Tail>(tail)...);
}

template<typename Fn, typename TagEqual, typename TagCombine>
template<typename Tags, std::size_t N, typename Head, typename... Tail>
void recursive_apply<Fn, TagEqual, TagCombine>::recursive_apply_(
    const Tags& tags,
    std::array<metric_value, N>& values,
    const std::reference_wrapper<Head>& head,
    Tail&&... tail) {
  this->recursive_apply_(tags, values, head.get(), std::forward<Tail>(tail)...);
}

template<typename Fn, typename TagEqual, typename TagCombine>
template<typename Tags, std::size_t N, typename... Head, typename... Tail>
void recursive_apply<Fn, TagEqual, TagCombine>::recursive_apply_(
    const Tags& tags,
    std::array<metric_value, N>& values,
    const std::variant<Head...>& head,
    Tail&&... tail) {
  std::visit(
      [&](const auto& head_val) {
        this->recursive_apply_(tags, values, head_val, std::forward<Tail>(tail)...);
      },
      head);
}

template<typename Fn, typename TagEqual, typename TagCombine>
template<std::size_t N, typename... Tail>
void recursive_apply<Fn, TagEqual, TagCombine>::recursive_apply_(
    const untagged&,
    std::array<metric_value, N>& values,
    const std::tuple<tags, metric_value>& head,
    Tail&&... tail) {
  this->recursive_apply_(
      std::get<0>(head),
      values,
      std::get<1>(head),
      std::forward<Tail>(tail)...);
}

template<typename Fn, typename TagEqual, typename TagCombine>
template<std::size_t N, typename... Tail>
void recursive_apply<Fn, TagEqual, TagCombine>::recursive_apply_(
    const tags& tag_set,
    std::array<metric_value, N>& values,
    const std::tuple<tags, metric_value>& head,
    Tail&&... tail) {
  if (tag_equal_(tag_set, std::get<0>(head))) {
    this->recursive_apply_(
        tag_combine_(tag_set, std::get<0>(head)),
        values,
        std::get<1>(head),
        std::forward<Tail>(tail)...);
  }
}

template<typename Fn, typename TagEqual, typename TagCombine>
template<typename Tags, std::size_t N, typename... Tail>
void recursive_apply<Fn, TagEqual, TagCombine>::recursive_apply_(
    const Tags& tags,
    std::array<metric_value, N>& values,
    const metric_value& head,
    Tail&&... tail) {
  values[N - 1u - sizeof...(Tail)] = head;
  this->recursive_apply_(
      tags,
      values,
      std::forward<Tail>(tail)...);
}

template<typename Fn, typename TagEqual, typename TagCombine>
template<std::size_t N, typename... MapArgs, typename... Tail>
void recursive_apply<Fn, TagEqual, TagCombine>::recursive_apply_(
    const untagged&,
    std::array<metric_value, N>& values,
    const std::unordered_map<tags, metric_value, MapArgs...>& head,
    Tail&&... tail) {
  for (const auto& head_elem : head) {
    this->recursive_apply_(
        std::get<0>(head_elem),
        values,
        std::get<1>(head_elem),
        std::forward<Tail>(tail)...);
  }
}

template<typename Fn, typename TagEqual, typename TagCombine>
template<std::size_t N, typename... MapArgs, typename... Tail>
void recursive_apply<Fn, TagEqual, TagCombine>::recursive_apply_(
    const tags& tag_set,
    std::array<metric_value, N>& values,
    const std::unordered_map<tags, metric_value, MapArgs...>& head,
    Tail&&... tail) {
  auto head_elem = head.find(tag_set);
  if (head_elem != head.end()) {
    this->recursive_apply_(
        tag_combine_(tag_set, std::get<0>(*head_elem)),
        values,
        std::get<1>(*head_elem),
        std::forward<Tail>(tail)...);
  }
}


template<typename Fn, typename TagEqual, typename TagCombine>
inline auto make_recursive_apply(Fn&& fn,
    TagEqual&& equal, TagCombine&& tag_combine)
-> recursive_apply<
    std::decay_t<Fn>,
    std::decay_t<TagEqual>,
    std::decay_t<TagCombine>> {
  using result_type = recursive_apply<
      std::decay_t<Fn>,
      std::decay_t<TagEqual>,
      std::decay_t<TagCombine>>;
  return result_type(
      std::forward<Fn>(fn),
      std::forward<TagEqual>(equal),
      std::forward<TagCombine>(tag_combine));
}

template<typename Fn>
inline auto make_recursive_apply(Fn&& fn)
-> recursive_apply<std::decay_t<Fn>> {
  using result_type = recursive_apply<std::decay_t<Fn>>;
  return result_type(std::forward<Fn>(fn));
}


template<std::size_t CbIdx>
template<typename Fn, typename... Managed, typename Value>
void merger_invocation<CbIdx>::operator()(
    Fn& fn,
    const std::tuple<Managed...>& managed,
    const Value& v) {
  static_assert(sizeof...(Managed) > CbIdx, "Index out of range of managed objpipes.");
  static_assert(std::is_same_v<expression::scalar_emit_type, Value>
      || std::is_same_v<expression::vector_emit_type, Value>,
      "Value must be a scalar or vector emit type.");
  invoke_(fn, managed, v, std::index_sequence_for<Managed...>());
}

template<std::size_t CbIdx>
template<typename Fn, typename... Managed, typename Value,
    std::size_t... Indices>
void merger_invocation<CbIdx>::invoke_(
    Fn& fn,
    const std::tuple<Managed...>& managed,
    const Value& v,
    std::index_sequence<Indices...> indices) {
  auto unpack = unpack_(v);
  unpack_invoke_(
      unpack,
      indices,
      fn,
      v.tp,
      this->template apply_unpack_<Indices>(
          unpack,
          this->template choose_value_<Indices>(
              std::get<Indices>(managed),
              v.data))...);
}

template<std::size_t CbIdx>
template<typename Fn, typename... Values, std::size_t... Indices>
void merger_invocation<CbIdx>::unpack_invoke_(
    const unpack_& unpack,
    std::index_sequence<Indices...>,
    Fn& fn,
    time_point tp,
    Values&&... values) {
  const bool factual = !unpack.speculative;

  make_recursive_apply(
      [&fn, &tp, factual](auto&&... args) {
        std::invoke(
            fn,
            factual,
            tp,
            std::forward<decltype(args)>(args)...);
      })
      (std::forward<Values>(values)...);
}

template<std::size_t CbIdx>
template<std::size_t Idx, typename Value>
auto merger_invocation<CbIdx>::apply_unpack_(
    unpack_& unpack,
    Value& value)
-> typename unpack_result_type<Value&, Idx == CbIdx>::type {
  if constexpr(Idx == CbIdx)
    return value;
  else
    return unpack(value);
}

template<std::size_t CbIdx>
template<std::size_t Idx, typename Managed, typename Value>
constexpr auto merger_invocation<CbIdx>::choose_value_(
    const Managed& managed,
    const Value& value)
-> std::conditional_t<
    Idx == CbIdx,
    const Value&,
    const typename Managed::accumulator_type&> {
  if constexpr(Idx == CbIdx) {
    return value;
  } else {
    return managed.accumulator;
  }
}


template<typename Fn, typename SpecInsIter, std::size_t N>
merger_acceptor<Fn, SpecInsIter, false, N>::merger_acceptor(
    Fn& fn,
    SpecInsIter speculative)
: fn_(fn),
  speculative(std::move(speculative))
{}

template<typename Fn, typename SpecInsIter, std::size_t N>
void merger_acceptor<Fn, SpecInsIter, false, N>::operator()(
    bool is_factual,
    time_point tp,
    const std::array<metric_value, N>& values) {
  using speculative_entry =
      typename merger_acceptor_data<false>::speculative_entry;

  if (is_factual) {
    assert(!this->factual.has_value());
    this->factual.emplace(tp, std::apply(fn_, values));
  } else {
    *speculative++ = speculative_entry(
        tp,
        std::apply(fn_, values));
  }
}


template<typename Fn, typename SpecInsIter, std::size_t N>
merger_acceptor<Fn, SpecInsIter, true, N>::merger_acceptor(Fn& fn, SpecInsIter speculative)
: fn_(fn),
  speculative(std::move(speculative))
{}

template<typename Fn, typename SpecInsIter, std::size_t N>
void merger_acceptor<Fn, SpecInsIter, true, N>::operator()(
    bool is_factual,
    time_point tp,
    const tags& tag_set,
    const std::array<metric_value, N>& values) {
  if (is_factual) {
    if (!this->factual.has_value()) {
      this->factual.emplace(
          std::piecewise_construct,
          std::forward_as_tuple(tp),
          std::forward_as_tuple());
    }
    assert(std::get<0>(*this->factual) == tp);

    std::get<1>(*this->factual).emplace(tag_set, std::apply(fn_, values));
  } else {
    *speculative++ = speculative_entry(
        std::piecewise_construct,
        std::forward_as_tuple(tp),
        std::forward_as_tuple(tag_set, std::apply(fn_, values)));
  }
}


} /* namespace monsoon::expressions::<unnamed> */


template<typename Fn, typename... ObjPipes>
merger<Fn, ObjPipes...>::read_invocation_::read_invocation_(invocation_fn fn) noexcept
: invocation_(fn)
{
  assert(invocation_ != nullptr);
}

template<typename Fn, typename... ObjPipes>
bool merger<Fn, ObjPipes...>::read_invocation_::is_pullable() const noexcept {
  return invocation_ != nullptr;
}

template<typename Fn, typename... ObjPipes>
void merger<Fn, ObjPipes...>::read_invocation_::operator()(merger& self) {
  assert(invocation_ != nullptr);

  bool pullable;
  std::tie(factual_until_, pullable) = std::invoke(invocation_, self);
  if (!pullable) invocation_ = nullptr;
}

template<typename Fn, typename... ObjPipes>
bool merger<Fn, ObjPipes...>::read_invocation_::operator<(
    const read_invocation_& other) const noexcept {
  if (invocation_ == nullptr) return other.invocation_ != nullptr;
  if (other.invocation_ == nullptr) return false;
  return factual_until_ < other.factual_until_;
}

template<typename Fn, typename... ObjPipes>
bool merger<Fn, ObjPipes...>::read_invocation_::operator>(
    const read_invocation_& other) const noexcept {
  return other < *this;
}


template<typename Fn, typename... ObjPipes>
merger<Fn, ObjPipes...>::merger(Fn&& fn, ObjPipes&&... inputs)
: managed_(std::move(inputs)...),
  fn_(std::move(fn))
{
  using namespace std::placeholders;

  assert(std::all_of(
          read_invocations_.begin(),
          read_invocations_.end(),
          std::bind(&read_invocation_::is_pullable, _1)));
}

template<typename Fn, typename... ObjPipes>
auto merger<Fn, ObjPipes...>::is_pullable() const noexcept -> bool {
  std::unique_lock<std::mutex> lck{ mtx_ };
  return !speculative_pending_.empty()
      || !factual_pending_.empty()
      || ex_pending_ != nullptr
      || read_invocations_.front().is_pullable();
}

template<typename Fn, typename... ObjPipes>
auto merger<Fn, ObjPipes...>::empty() const noexcept -> bool {
  std::unique_lock<std::mutex> lck{ mtx_ };
  if (!speculative_pending_.empty()
      || !factual_pending_.empty()
      || ex_pending_ != nullptr) return false;

  const_cast<merger&>(*this).try_fill_();
  return speculative_pending_.empty()
      && factual_pending_.empty()
      && ex_pending_ == nullptr;
}

template<typename Fn, typename... ObjPipes>
auto merger<Fn, ObjPipes...>::pull(objpipe_errc& e)
-> std::optional<value_type> {
  std::unique_lock<std::mutex> lck{ mtx_ };

  do {
    std::optional<value_type> result = try_pull_(e);
    if (result.has_value()) return result;
    if (!result.has_value() && e == objpipe_errc::success)
      e = wait_(lck);
  } while (e == objpipe_errc::success);
  return {};
}

template<typename Fn, typename... ObjPipes>
auto merger<Fn, ObjPipes...>::try_pull(objpipe_errc& e)
-> std::optional<value_type> {
  std::unique_lock<std::mutex> lck{ mtx_ };

  return try_pull_(e);
}

template<typename Fn, typename... ObjPipes>
auto merger<Fn, ObjPipes...>::try_pull_(objpipe_errc& e)
-> std::optional<value_type> {
  e = objpipe_errc::success;
  std::optional<value_type> result;

  try_fill_();
  if (ex_pending_) std::rethrow_exception(ex_pending_);

  if (!factual_pending_.empty()) {
    result.emplace(
        std::move(factual_pending_.front().first),
        std::in_place_index<1>,
        std::move(factual_pending_.front().second));
    factual_pending_.pop_front();
  } else if (!speculative_pending_.empty()) {
    result.emplace(
        std::move(speculative_pending_.front().first),
        std::in_place_index<0>,
        std::move(speculative_pending_.front().second));
    speculative_pending_.pop_front();
  } else {
    bool all_closed = true;
    for_each_managed_(
        [&all_closed](const auto& man) {
          all_closed &= !man.is_pullable();
        });
    if (all_closed) e = objpipe_errc::closed;
  }

  return result;
}

template<typename Fn, typename... ObjPipes>
void merger<Fn, ObjPipes...>::try_fill_() noexcept {
  if (!factual_pending_.empty()) return;

  auto end = read_invocations_.end();
  while (factual_pending_.empty()
      && ex_pending_ == nullptr
      && end != read_invocations_.begin()
      && read_invocations_.front().is_pullable()) {
    std::pop_heap(read_invocations_.begin(), end, std::greater<>());
    --end;

    try {
      (*end)(*this);
    } catch (...) {
      ex_pending_ = std::current_exception();
    }
  }

  while (end != read_invocations_.end())
    std::push_heap(read_invocations_.begin(), ++end, std::greater<>());
}

template<typename Fn, typename... ObjPipes>
template<std::size_t CbIdx>
auto merger<Fn, ObjPipes...>::load_until_next_factual_()
-> std::tuple<std::optional<time_point>, bool> {
  using namespace std::placeholders;
  using spec_insert_iter =
      decltype(std::back_inserter(speculative_pending_));

  merger_acceptor<const Fn, spec_insert_iter, tagged, sizeof...(ObjPipes)> acceptor{
      fn_,
      std::back_inserter(speculative_pending_)
  };
  auto& managed = std::get<CbIdx>(managed_);
  const bool found_factual = managed.load_until_next_factual(
      std::bind(
          merger_invocation<CbIdx>(),
          std::ref(acceptor),
          std::cref(managed_),
          _1));

  // Commit acceptor.
  if (acceptor.factual.has_value()) {
    assert(found_factual);
    const time_point tp = acceptor.factual->first;
    factual_pending_.push_back(*std::move(acceptor.factual));

    for_each_managed_([tp](auto& m) { m.advance_factual(tp); });
    speculative_pending_.erase(
        std::remove_if(
            speculative_pending_.begin(),
            speculative_pending_.end(),
            [tp](const auto& entry) {
              return entry.first <= tp;
            }),
        speculative_pending_.end());
  }

  return std::make_tuple(managed.factual_until(), managed.is_pullable());
}

template<typename Fn, typename... ObjPipes>
template<typename ManFn>
void merger<Fn, ObjPipes...>::for_each_managed_(ManFn fn) {
  for_each_managed_impl_(fn, std::index_sequence_for<ObjPipes...>());
}

template<typename Fn, typename... ObjPipes>
template<typename ManFn>
void merger<Fn, ObjPipes...>::for_each_managed_impl_(
    ManFn&,
    std::index_sequence<>) noexcept
{}

template<typename Fn, typename... ObjPipes>
template<typename ManFn, std::size_t Idx, std::size_t... Tail>
void merger<Fn, ObjPipes...>::for_each_managed_impl_(
    ManFn& fn,
    std::index_sequence<Idx, Tail...>) {
  std::invoke(fn, std::get<Idx>(managed_));
  for_each_managed_impl_(fn, std::index_sequence<Tail...>());
}

template<typename Fn, typename... ObjPipes>
template<std::size_t... Indices>
constexpr auto merger<Fn, ObjPipes...>::new_read_invocations_(
    std::index_sequence<Indices...>) noexcept
-> std::array<read_invocation_, sizeof...(Indices)> {
  return {{ read_invocation_(&merger::template load_until_next_factual_<Indices>)... }};
}


template<typename Fn, typename... ObjPipe>
auto make_merger(Fn&& fn, ObjPipe&&... inputs)
-> objpipe::reader<typename merger<
    std::decay_t<Fn>,
    std::decay_t<ObjPipe>...>::value_type> {
  return {
      std::forward<Fn>(fn),
      new merger<std::decay_t<ObjPipe>...>{ std::forward<ObjPipe>(inputs)... }
  };
}


}} /* namespace monsoon::expressions */

#endif /* MONSOON_EXPRESSIONS_MERGER_INL_H */
