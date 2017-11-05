#include <monsoon/expressions/merger.h>
#include <monsoon/interpolate.h>

namespace monsoon {
namespace expressions {


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
    std::reference_wrapper<const expression::factual_vector>> {
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
    return std::cref(at_after->second);

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
      [&interpolated, tp, before_tp, after_tp, &after_map](
	  const auto& tagged_metric_before) {
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


auto unpack_::operator()(const scalar_accumulator& m)
-> std::optional<metric_value> {
  std::optional<std::tuple<metric_value, bool>> opt_mv = m[tp];
  if (!opt_mv.has_value()) return {};
  speculative |= !std::get<1>(*opt_mv);
  return std::get<0>(*std::move(opt_mv));
}

auto unpack_::operator()(const vector_accumulator& m)
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


template class merger_managed<expression::scalar_objpipe>;
template class merger_managed<expression::vector_objpipe>;

template
auto make_merger(
    metric_value(*const&)(const metric_value&, const metric_value&),
    expression::scalar_objpipe&&,
    expression::scalar_objpipe&&)
    -> objpipe::reader<typename merger<
        std::decay_t<metric_value(*)(const metric_value&, const metric_value&)>,
        std::decay_t<expression::scalar_objpipe&&>,
        std::decay_t<expression::scalar_objpipe&&>>::value_type>;

template
auto make_merger(
    metric_value(*const&)(const metric_value&, const metric_value&),
    expression::vector_objpipe&&,
    expression::scalar_objpipe&&)
    -> objpipe::reader<typename merger<
        std::decay_t<metric_value(*)(const metric_value&, const metric_value&)>,
        std::decay_t<expression::vector_objpipe&&>,
        std::decay_t<expression::scalar_objpipe&&>>::value_type>;

template
auto make_merger(
    metric_value(*const&)(const metric_value&, const metric_value&),
    expression::scalar_objpipe&&,
    expression::vector_objpipe&&)
    -> objpipe::reader<typename merger<
        std::decay_t<metric_value(*)(const metric_value&, const metric_value&)>,
        std::decay_t<expression::scalar_objpipe&&>,
        std::decay_t<expression::vector_objpipe&&>>::value_type>;

template
auto make_merger(
    metric_value(*const&)(const metric_value&, const metric_value&),
    expression::vector_objpipe&&,
    expression::vector_objpipe&&)
    -> objpipe::reader<typename merger<
        std::decay_t<metric_value(*)(const metric_value&, const metric_value&)>,
        std::decay_t<expression::vector_objpipe&&>,
        std::decay_t<expression::vector_objpipe&&>>::value_type>;


}} /* namespace monsoon::expressions */
