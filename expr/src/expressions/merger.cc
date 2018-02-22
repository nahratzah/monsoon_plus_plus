///\file
///\ingroup expr

#include <monsoon/expressions/merger.h>
#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <tuple>
#include <variant>
#include <monsoon/interpolate.h>
#include <monsoon/metric_value.h>
#include <monsoon/overload.h>
#include <monsoon/tags.h>
#include <monsoon/time_point.h>
#include <monsoon/expression.h>
#include <monsoon/objpipe/of.h>
#include <monsoon/expr_export_.h>

namespace monsoon {
namespace expressions {


/**
 * \brief Scalar time point value.
 * \ingroup expr
 * \details Describes a scalar at a given time point.
 */
struct monsoon_expr_local_ scalar {
  ///\brief True if this is factual information, false otherwise.
  bool is_fact = false;
  ///\brief Contains the metric value.
  ///\details Absence indicates no value is available.
  ///\note An absent fact, means it is known that no value will be available.
  std::optional<metric_value> value;

  ///\brief Default constructor creates a tp_value holding no data.
  scalar() = default;

  ///\brief Create a tp_value holding a copy of the given data.
  ///\param[in] value The value to hold.
  ///\param[in] is_fact Indicates if the data is factual.
  scalar(metric_value&& value, bool is_fact)
  noexcept(std::is_nothrow_move_constructible_v<metric_value>)
  : is_fact(is_fact),
    value(std::move(value))
  {}

  ///\brief Create a tp_value holding a copy of the given data.
  ///\param[in] value The value to hold.
  ///\param[in] is_fact Indicates if the data is factual.
  scalar(const metric_value& value, bool is_fact)
  noexcept(std::is_nothrow_copy_constructible_v<metric_value>)
  : is_fact(is_fact),
    value(value)
  {}

  ///\brief Create a tp_value holding a copy of the given data.
  ///\param[in] value The value to hold.
  ///\param[in] is_fact Indicates if the data is factual.
  scalar(std::optional<metric_value>&& value, bool is_fact)
  noexcept(std::is_nothrow_move_constructible_v<metric_value>)
  : is_fact(is_fact),
    value(std::move(value))
  {}

  ///\brief Create a tp_value holding a copy of the given data.
  ///\param[in] value The value to hold.
  ///\param[in] is_fact Indicates if the data is factual.
  scalar(const std::optional<metric_value>& value, bool is_fact)
  noexcept(std::is_nothrow_copy_constructible_v<metric_value>)
  : is_fact(is_fact),
    value(value)
  {}

  ///\brief Creates a scalar without a value.
  ///\param[in] is_fact Indicates that the absence of a value is a fact.
  static auto absent(bool is_fact)
  noexcept
  -> scalar {
    scalar rv;
    rv.is_fact = is_fact;
    return rv;
  }
};

/**
 * \brief Describes a tagged vector.
 * \ingroup expr
 * \details Describes a set of tagged values at a given time point.
 */
struct monsoon_expr_local_ tagged_vector {
  using map_type = std::unordered_map<
      tags, metric_value,
      class match_clause::hash,
      match_clause::equal_to>;

  ///\copydoc scalar::is_fact
  bool is_fact = false;
  ///\copydoc scalar::value
  map_type values;

  ///\brief Create an empty tagged vector.
  ///\param[in] bucket_count The number of initial buckets to create.
  ///\param[in] hash The hash function for the map.
  ///\param[in] eq The equality function for the map.
  ///\param[in] is_fact Indicates if the data is factual.
  tagged_vector(map_type::size_type bucket_count,
      class match_clause::hash hash,
      match_clause::equal_to eq,
      bool is_fact)
  : is_fact(is_fact),
    values(bucket_count, std::move(hash), std::move(eq))
  {}
};

class monsoon_expr_local_ scalar_sink {
 private:
  struct value {
    value() = delete;

    value(time_point tp, const metric_value& data, bool is_fact)
    : tp(tp),
      data(data),
      is_fact(is_fact)
    {}

    value(time_point tp, metric_value&& data, bool is_fact) noexcept
    : tp(tp),
      data(std::move(data)),
      is_fact(is_fact)
    {}

    time_point tp;
    expression::speculative_scalar data;
    bool is_fact;
  };

  ///\brief Type of the data_ member.
  using data_type = std::deque<value>;

  struct recent_type {
    recent_type() = delete;

    recent_type(time_point tp, const metric_value& data)
    : tp(tp),
      data(data)
    {}

    recent_type(time_point tp, metric_value&& data) noexcept
    : tp(tp),
      data(std::move(data))
    {}

    time_point tp;
    expression::speculative_scalar data;
  };

  class tp_compare_ {
   public:
    template<typename X, typename Y>
    auto operator()(const X& x, const Y& y) const
    noexcept
    -> bool {
      return tp_(x) < tp_(y);
    }

   private:
    static constexpr auto tp_(const time_point& tp)
    noexcept
    -> const time_point& {
      return tp;
    }

    static constexpr auto tp_(const value& x)
    noexcept
    -> const time_point& {
      return x.tp;
    }
  };

 public:
  scalar_sink();
  ~scalar_sink() noexcept;

  /**
   * \brief Suggest the next emit time point for factual emit.
   *
   * \returns The time point of the oldest factual time point held.
   */
  auto suggest_emit_tp() const noexcept -> std::optional<time_point>;

  /**
   * \brief Retrieve value at the given time point.
   *
   * \details The value may be interpolated, if needed.
   * \param[in] tp The time point for which a value is requested.
   * \param[in] max_interp_tp The max time point which may be considered when interpolating.
   * \param[in] is_closed Indicates the source of values is closed.
   * \returns Value at the given time point.
   */
  auto get(time_point tp, time_point max_interp_tp, bool is_closed) const -> scalar;

  /**
   * \brief Test if this holds any data.
   *
   * \details If this holds no data, it should be equal to a newly constructed instance.
   */
  auto empty() const noexcept -> bool;

  /**
   * \brief Ensure the buffered, emittable time points are all after the given time point.
   *
   * \details Ensures the only pending data is after the given time point.
   * \param[in] tp The time point to forward to.
   * \param[in] expire_before All data before this time point will be dropped,
   * keeping it from being used in future interpolations.
   */
  auto forward_to_time(time_point tp, time_point expire_before) -> void;

  /**
   * \brief Accept an emited scalar.
   *
   * \details Adds the emitted scalar to the internal buffer.
   * \param[in] emt The scalar to add to the buffer.
   * \return True if the scalar was added, false otherwise.
   */
  auto accept(expression::scalar_emit_type&& emt) -> bool;

  /**
   * \brief Invariant.
   *
   * \details Tests if the invariant holds.
   */
  auto invariant() const noexcept -> bool;

 private:
  /**
   * \brief List of factual values.
   *
   * \details
   * List is ordered by time_point.
   * Each element has a time point after recent_->tp.
   *
   * Time points are unique.
   *
   * Furthermore, the list is a partition, with factual time points
   * preceding speculative ones.
   * (In other words: each factual value only has factual values preceding it,
   * and each speculative value only has speculative values following it.)
   */
  data_type data_;

  /**
   * \brief Last factual before the forwarding time point.
   *
   * \details
   * This factual has a time point before factual_.front().tp.
   *
   * It is used in interpolations between it and factual_.front().
   */
  std::optional<recent_type> recent_;
};

class monsoon_expr_local_ vector_sink {
 public:
  vector_sink(const std::shared_ptr<const match_clause>& mc)
  : data_(0, mc, mc)
  {}

  /**
   * \brief Suggest the next emit time point for factual emit.
   *
   * \returns The time point of the oldest factual time point held.
   */
  auto suggest_emit_tp() const noexcept -> std::optional<time_point>;

  /**
   * \brief Retrieve value at the given time point.
   *
   * \details The value may be interpolated, if needed.
   * \param[in] tp The time point for which a value is requested.
   * \param[in] max_interp_tp The max time point which may be considered when interpolating.
   * \param[in] is_closed Indicates the source of values is closed.
   * \returns Value at the given time point.
   */
  auto get(time_point tp, time_point max_interp_tp, bool is_closed) const -> tagged_vector;

  /**
   * \brief Test if this holds any data.
   *
   * \details If this holds no data, it should be equal to a newly constructed instance.
   */
  auto empty() const noexcept -> bool;

  /**
   * \brief Ensure the buffered, emittable time points are all after the given time point.
   *
   * \details Ensures the only pending data is after the given time point.
   * \param[in] tp The time point to forward to.
   * \param[in] expire_before All data before this time point will be dropped,
   * keeping it from being used in future interpolations.
   */
  auto forward_to_time(time_point tp, time_point expire_before) -> void;

  /**
   * \brief Accept an emited vector.
   *
   * \details Adds the emitted vector to the internal buffer.
   * \param[in] emt The vector to add to the buffer.
   * \return True if the vector was added, false otherwise.
   */
  auto accept(expression::vector_emit_type&& emt) -> bool;

  /**
   * \brief Invariant.
   *
   * \details Tests if the invariant holds.
   */
  auto invariant() const noexcept -> bool;

 private:
  /**
   * \brief Collection of tagged scalars.
   */
  std::unordered_map<tags, scalar_sink, class match_clause::hash, match_clause::equal_to>
      data_;
};


/**
 * \brief Wrapper that connects a source and its associated sink.
 * \ingroup expr
 * \details Reads data from the \p Pipe (an \ref monsoon::objpipe::reader<T> "objpipe")
 * into the appropriate sink.
 *
 * \tparam Pipe An object pipe. Must be an instance of
 *   \ref monsoon::expression::scalar_objpipe "scalar_objpipe"
 *   or \ref monsoon::expression::vector_objpipe "vector_objpipe".
 *
 * To be more precise, the template relies on the value type matching.
 */
template<typename Pipe>
class monsoon_expr_local_ pull_cycle {
 public:
  ///\brief The sink type used by the pull cycle.
  ///\details One of \ref scalar_objpipe or \ref vector_objpipe.
  using sink_type = typename std::conditional_t<
          std::is_same_v<expression::scalar_emit_type, typename Pipe::value_type>,
          std::enable_if<
              std::is_same_v<expression::scalar_emit_type, typename Pipe::value_type>,
              scalar_sink>,
          std::enable_if<
              std::is_same_v<expression::vector_emit_type, typename Pipe::value_type>,
              vector_sink>>
      ::type;

  /**
   * \brief Constructs a new cycle, taking values from
   * the \ref monsoon::objpipe::reader<T> "objpipe" \p source
   * and storing them into \p sink.
   */
  pull_cycle(Pipe&& source, sink_type&& sink = sink_type())
  noexcept(std::is_nothrow_move_constructible_v<Pipe>
      && std::is_nothrow_move_constructible_v<sink_type>)
  : source_(std::move(source)),
    sink_(std::move(sink))
  {}

  /**
   * \brief Suggest the next emit time point for factual emit.
   *
   * \returns The time point of the oldest factual time point held.
   */
  auto suggest_emit_tp() const noexcept -> std::optional<time_point>;

  /**
   * \brief Retrieve value at the given time point.
   *
   * \details Invokes
   * \code
   * sink.get(tp, max_interp_tp);
   * \endcode
   * \param tp The time point for which a value is requested.
   * \param max_interp_tp The max time point which may be considered when interpolating.
   * \returns Value at the given time point.
   */
  auto get(time_point tp, time_point max_interp_tp) const
  -> decltype(std::declval<const sink_type&>()
      .get(std::declval<time_point>(), std::declval<time_point>(), std::declval<bool>()));

  ///\brief Read data from source, transferring it into sink.
  ///\details Stops reading if data fails to read, or after a fact was transferred.
  void read_more();

 private:
  sink_type sink_;
  Pipe source_;
};

extern template class monsoon_expr_local_ pull_cycle<expression::scalar_objpipe>;
extern template class monsoon_expr_local_ pull_cycle<expression::vector_objpipe>;


scalar_sink::scalar_sink() {}

scalar_sink::~scalar_sink() noexcept {}

auto scalar_sink::suggest_emit_tp() const
noexcept
-> std::optional<time_point> {
  assert(invariant());

  if (data_.empty() || !data_.front().is_fact) return {};
  return data_.front().tp;
}

auto scalar_sink::get(time_point tp, time_point max_interp_tp,
    bool is_closed) const
-> scalar {
  assert(tp <= max_interp_tp);
  assert(invariant());

  const auto at_or_successor = std::lower_bound(
      data_.begin(), data_.end(),
      tp,
      tp_compare_());
  // If closed, only facts are to be evaluated.
  if (is_closed && (at_or_successor == data_.end() || !at_or_successor->is_fact))
    return scalar::absent(true);

  if (at_or_successor == data_.end())
    return scalar::absent(false); // Indicate absence of result is speculative.

  if (at_or_successor->tp == tp) // Exact match for tp.
    return scalar(at_or_successor->data, at_or_successor->is_fact);
  else if (at_or_successor->tp > max_interp_tp)
    return scalar::absent(at_or_successor->is_fact);

  // at_or_successor is a successor.
  if (at_or_successor == data_.begin()) { // Interpolate using stored most-recent fact.
    if (!recent_.has_value())
      return scalar::absent(at_or_successor->is_fact);
    return scalar(
        interpolate(tp,
            { recent_->tp, recent_->data },
            { at_or_successor->tp, at_or_successor->data }),
        at_or_successor->is_fact);
  } else { // Interpolate using predecessor.
    const auto predecessor = std::prev(at_or_successor);
    // Note: at_or_successor->is_fact implies predecessor->is_fact
    assert(predecessor->is_fact || !at_or_successor->is_fact);

    return scalar(
        interpolate(tp,
            { predecessor->tp, predecessor->data },
            { at_or_successor->tp, at_or_successor->data }),
        at_or_successor->is_fact);
  }
  // unreachable
}

auto scalar_sink::empty() const
noexcept
-> bool {
  assert(invariant());

  return data_.empty() && !recent_.has_value();
}

auto scalar_sink::forward_to_time(time_point tp, time_point expire_before)
-> void {
  assert(invariant());

  if (recent_.has_value() && recent_->tp < expire_before)
    recent_.reset();

  const auto keep_begin = std::lower_bound(
      data_.begin(),
      data_.end(),
      tp,
      tp_compare_());

  if (keep_begin != data_.begin()) {
    const auto first_speculative = std::upper_bound(
        data_.begin(), keep_begin,
        true,
        overload(
            [](const value& x, bool y) { return x.is_fact > y; },
            [](bool x, const value& y) { return x > y.is_fact; }));
    if (first_speculative != data_.begin()) {
      const auto last_dropped_fact = std::prev(first_speculative);
      assert(last_dropped_fact->is_fact);
      if (last_dropped_fact->tp >= expire_before)
        recent_.emplace(last_dropped_fact->tp, std::move(last_dropped_fact->data));
    }
  }

  // Validate that we don't erase any facts at/after expire_before,
  // which should have been stored in recent_ instead.
  assert(std::none_of(
          data_.begin(), keep_begin,
          [this, expire_before](const value& x) {
            return x.is_fact
                && ((recent_.has_value() && x.tp > recent_->tp)
                    || (x.tp >= expire_before && x.is_fact));
          }));
  data_.erase(data_.begin(), keep_begin);

  assert(invariant());
  assert(!recent_.has_value() || recent_->tp >= expire_before);
  assert(data_.empty() || data_.front().tp >= tp);
}

auto scalar_sink::accept(expression::scalar_emit_type&& emt)
-> bool {
  assert(invariant());

  data_type::iterator ins_pos;
  const time_point tp = emt.tp;
  metric_value&& mv = (emt.data.index() == 0
      ? std::get<0>(std::move(emt.data))
      : std::get<1>(std::move(emt.data)));
  const bool is_fact = (emt.data.index() == 1);

  if (recent_.has_value() && tp < recent_->tp) {
    assert(invariant());
    return false;
  }

  const auto at_or_successor = std::lower_bound(data_.begin(), data_.end(),
      tp,
      tp_compare_());
  if (at_or_successor->tp == tp) {
    assert(!at_or_successor->is_fact);
    std::tie(at_or_successor->data, at_or_successor->is_fact) =
        std::forward_as_tuple(std::move(mv), is_fact);

    ins_pos = at_or_successor;
  } else if (!at_or_successor->is_fact || is_fact) {
    ins_pos = data_.emplace(at_or_successor, tp, std::move(mv), is_fact);
    // Potentially invalidates at_or_successor.
  } else {
    assert(invariant());
    return false; // Reject speculative value before most recent fact.
  }
  // Note: at_or_successor may be invalidated.

  if (is_fact) { // Erase all speculative values before factual insertion.
    const auto first_spec = std::upper_bound(
        data_.begin(), ins_pos,
        true,
        overload(
            [](const value& x, bool y) { return x.is_fact > y; },
            [](bool x, const value& y) { return x > y.is_fact; }));
    data_.erase(first_spec, ins_pos);
  }

  assert(invariant());
  return true;
}

auto scalar_sink::invariant() const
noexcept
-> bool {
  if (recent_.has_value() && !data_.empty()
      && recent_->tp >= data_.front().tp)
    return false;

  // Find partition point where speculative data starts.
  const auto speculative_begin = std::lower_bound(
      data_.begin(), data_.end(),
      false,
      overload( // Compare true before false.
          [](const value& x, bool y) noexcept { return x.is_fact > y; },
          [](bool x, const value& y) noexcept { return x > y.is_fact; }));

  return
      std::all_of(
          data_.begin(), speculative_begin,
          std::mem_fn(&value::is_fact))
      && std::none_of(
          speculative_begin, data_.end(),
          std::mem_fn(&value::is_fact))
      && std::is_sorted(
          data_.begin(), data_.end(),
          tp_compare_());
}


auto vector_sink::suggest_emit_tp() const
noexcept
-> std::optional<time_point> {
  return objpipe::of(std::cref(data_))
      .iterate()
      .select_second()
      .transform(&scalar_sink::suggest_emit_tp)
#if 0 // Work around libc++ bug: https://bugs.llvm.org/show_bug.cgi?id=36469
      .filter(&std::optional<time_point>::has_value)
#else
      .filter([](const std::optional<time_point>& v) { return v.has_value(); })
#endif
      .deref()
      .min();
}

auto vector_sink::get(time_point tp, time_point max_interp_tp,
    bool is_closed) const
-> tagged_vector {
  assert(invariant());

  tagged_vector result = tagged_vector(
      data_.bucket_count(),
      data_.hash_function(),
      data_.key_eq(),
      true);

  std::for_each(
      data_.begin(), data_.end(),
      [&result, tp, max_interp_tp, is_closed](const auto& elem) {
        const auto& t = elem.first;
        auto s = elem.second.get(tp, max_interp_tp, is_closed);

        if (!s.is_fact) result.is_fact = false;
        if (s.value.has_value())
          result.values.emplace(t, *std::move(s.value));
      });
  return result;
}

auto vector_sink::empty() const
noexcept
-> bool {
  assert(invariant());

  return data_.empty();
}

auto vector_sink::forward_to_time(time_point tp, time_point expire_before)
-> void {
  assert(invariant());

  auto elem = data_.begin();
  auto elem_end = data_.end();
  while (elem != elem_end) {
    elem->second.forward_to_time(tp, expire_before);
    if (elem->second.empty())
      elem = data_.erase(elem);
    else
      ++elem;
  }

  assert(invariant());
}

auto vector_sink::accept(expression::vector_emit_type&& emt)
-> bool {
  assert(invariant());

  const time_point tp = emt.tp;
  bool accepted = std::visit(
      overload(
          [this, &tp](expression::speculative_vector&& v) {
            return data_[std::get<0>(std::move(v))]
                .accept(expression::scalar_emit_type(tp, std::in_place_index<0>, std::get<1>(std::move(v))));
          },
          [this, &tp](expression::factual_vector&& v) {
            bool accepted = false;
#if __cplusplus >= 201703
            while (!v.empty()) {
              auto nh = v.extract(v.begin());
              if (data_[std::move(nh.key())]
                  .accept(expression::scalar_emit_type(tp, std::in_place_index<1>, std::move(nh.mapped()))))
                accepted = true;
            }
#else
            for (auto& item : v) {
              if (data_[item.first]
                  .accept(expression::scalar_emit_type(tp, std::in_place_index<1>, std::move(item.second))))
                accepted = true;
            }
#endif
            return accepted;
          }),
      std::move(emt.data));

  assert(invariant());
  return accepted;
}

auto vector_sink::invariant() const
noexcept
-> bool {
  return objpipe::of(std::ref(data_))
      .iterate()
      .select_second()
      .transform(&scalar_sink::empty)
      .transform(std::logical_not<bool>())
      .reduce(std::logical_and<bool>())
      .value_or(true);
}


template<typename Pipe>
auto pull_cycle<Pipe>::suggest_emit_tp() const noexcept
-> std::optional<time_point> {
  return sink_.suggest_emit_tp();
}

template<typename Pipe>
auto pull_cycle<Pipe>::get(time_point tp, time_point max_interp_tp) const
-> decltype(std::declval<const sink_type&>()
    .get(std::declval<time_point>(), std::declval<time_point>(), std::declval<bool>())) {
  return sink_.get(tp, max_interp_tp, !source_.is_pullable());
}

template<typename Pipe>
auto pull_cycle<Pipe>::read_more()
-> void {
  bool accepted_fact = false;
  do {
    auto next = source_.try_pull();
    if (!next.has_value()) break;
    const bool is_fact = (next->data.index() == 1);

    const bool accepted = sink_.accept(*std::move(next));
    accepted_fact = (accepted && is_fact);
  } while (!accepted_fact);
}

template class monsoon_expr_local_ pull_cycle<expression::scalar_objpipe>;
template class monsoon_expr_local_ pull_cycle<expression::vector_objpipe>;


}} /* namespace monsoon::expressions */
