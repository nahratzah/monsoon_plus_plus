///\file
///\ingroup expr

#include <monsoon/expressions/merger.h>
#include <algorithm>
#include <deque>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <tuple>
#include <variant>
#include <vector>
#include <monsoon/interpolate.h>
#include <monsoon/metric_value.h>
#include <monsoon/overload.h>
#include <monsoon/tags.h>
#include <monsoon/time_point.h>
#include <monsoon/expression.h>
#include <monsoon/objpipe/of.h>
#include <monsoon/expr_export_.h>

namespace monsoon::expressions {
namespace {


/**
 * \brief Scalar time point value.
 * \ingroup expr
 * \details Describes a scalar at a given time point.
 */
struct scalar {
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
struct tagged_vector {
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

struct merger_apply_scalar {
  std::optional<metric_value> value;
  bool is_fact;

  merger_apply_scalar(metric_value&& value, bool is_fact)
  noexcept
  : value(std::move(value)),
    is_fact(is_fact)
  {}

  merger_apply_scalar(const metric_value& value, bool is_fact)
  : value(value),
    is_fact(is_fact)
  {}

  merger_apply_scalar(std::nullptr_t, bool is_fact)
  : value(),
    is_fact(is_fact)
  {}

  ///\brief Convert the scalar to a \ref expression::scalar_emit_type "scalar emit type".
  auto as_emit(time_point tp) &&
  -> std::vector<expression::scalar_emit_type> {
    std::vector<expression::scalar_emit_type> result;
    if (value.has_value()) {
      if (is_fact)
        result.emplace_back(tp, std::in_place_index<1>, *std::move(value));
      else
        result.emplace_back(tp, std::in_place_index<0>, *std::move(value));
    }
    return result;
  }
};

struct merger_apply_vector {
  using map_type = std::unordered_map<
      tags, metric_value,
      class match_clause::hash,
      match_clause::equal_to>;

  map_type values;
  bool is_fact;

  ///\brief Create an empty tagged vector.
  ///\param[in] bucket_count The number of initial buckets to create.
  ///\param[in] hash The hash function for the map.
  ///\param[in] eq The equality function for the map.
  ///\param[in] is_fact Indicates if the data is factual.
  merger_apply_vector(map_type::size_type bucket_count,
      class match_clause::hash hash,
      match_clause::equal_to eq,
      bool is_fact)
  : values(bucket_count, std::move(hash), std::move(eq)),
    is_fact(is_fact)
  {}

  ///\brief Convert the vector to a \ref expression::vector_emit_type "vector emit type".
  auto as_emit(time_point tp) &&
  -> std::vector<expression::vector_emit_type> {
    std::vector<expression::vector_emit_type> result;
    if (is_fact) {
      result.emplace_back(tp, std::in_place_index<1>, std::move(values));
    } else {
      result.reserve(values.size());
      for (auto& value : values)
        result.emplace_back(tp, std::in_place_index<0>, std::move(value.first), std::move(value.second));
    }
    return result;
  }
};

template<typename Fn>
auto merger_apply(Fn&& fn, scalar x, scalar y,
    [[maybe_unused]] const std::shared_ptr<const match_clause>& out_mc)
-> merger_apply_scalar {
  if (x.value.has_value() && y.value.has_value()) {
    return merger_apply_scalar(
        std::invoke(std::forward<Fn>(fn), x.value.value(), y.value.value()),
        x.is_fact && y.is_fact);
  } else {
    return merger_apply_scalar(
        nullptr,
        x.is_fact && y.is_fact);
  }
}

template<typename Fn>
auto merger_apply(Fn&& fn, scalar x, tagged_vector y,
    const std::shared_ptr<const match_clause>& out_mc)
-> merger_apply_vector {
  merger_apply_vector result = merger_apply_vector(
      y.values.bucket_count(),
      out_mc,
      out_mc,
      x.is_fact && y.is_fact);

  if (x.value.has_value()) {
    objpipe::of(std::move(y))
        .transform(&tagged_vector::values)
        .iterate()
        .transform(
            [&fn, &x](const auto& pair) {
              return std::make_pair(pair.first, std::invoke(fn, x.value.value(), pair.second));
            })
        .filter([&out_mc](const auto& pair) { return out_mc->pass(pair.first); })
        .for_each(
            [&result](std::pair<tags, metric_value>&& pair) {
              auto ins_pos = result.values.emplace(pair.first, std::move(pair.second));
              if (ins_pos.second == false)
                ins_pos.first->second = metric_value(); // Invalidate collision.
            });
  }

  return result;
}

template<typename Fn>
auto merger_apply(Fn&& fn, tagged_vector x, scalar y,
    const std::shared_ptr<const match_clause>& out_mc)
-> merger_apply_vector {
  merger_apply_vector result = merger_apply_vector(
      x.values.bucket_count(),
      out_mc,
      out_mc,
      x.is_fact && y.is_fact);

  if (y.value.has_value()) {
    objpipe::of(std::move(x))
        .transform(&tagged_vector::values)
        .iterate()
        .transform(
            [&fn, &y](const auto& pair) {
              return std::make_pair(pair.first, std::invoke(fn, pair.second, y.value.value()));
            })
        .filter([&out_mc](const auto& pair) { return out_mc->pass(pair.first); })
        .for_each(
            [&result](std::pair<tags, metric_value>&& pair) {
              auto ins_pos = result.values.emplace(std::move(pair.first), std::move(pair.second));
              if (ins_pos.second == false)
                ins_pos.first->second = metric_value(); // Invalidate collision.
            });
  }

  return result;
}

template<typename Fn>
auto merger_apply(Fn&& fn, tagged_vector x, tagged_vector y,
    const std::shared_ptr<const match_clause>& out_mc)
-> merger_apply_vector {
  const std::shared_ptr<const match_clause> mc = x.values.hash_function().mc;
  merger_apply_vector result = merger_apply_vector(
      x.values.bucket_count(),
      out_mc,
      out_mc,
      x.is_fact && y.is_fact);

  objpipe::of(std::move(x))
      .transform(&tagged_vector::values)
      .iterate()
      .transform(
          [&fn, &y, &mc](const std::pair<const tags, metric_value>& pair) -> std::optional<std::pair<tags, metric_value>> {
            auto y_val = y.values.find(pair.first);
            if (y_val == y.values.end()) return {};
            return std::make_pair(
                mc->reduce(pair.first, y_val->first),
                std::invoke(fn, pair.second, y_val->second));
          })
      .filter([](const auto& opt_pair) { return opt_pair.has_value(); })
      .deref()
      .filter([&out_mc](const auto& pair) { return out_mc->pass(pair.first); })
      .for_each(
          [&result](std::pair<tags, metric_value>&& pair) {
            auto ins_pos = result.values.emplace(std::move(pair.first), std::move(pair.second));
            if (ins_pos.second == false)
              ins_pos.first->second = metric_value(); // Invalidate collision.
          });

  return result;
}

/**
 * \brief Scalar sink.
 * \ingroup expr
 *
 * \details
 * Accepts scalars and stores them.
 * Stored scalars are used for emitting and interpolating.
 */
class scalar_sink {
 private:
  struct value {
    value() = delete;

    ///\brief Constructor.
    ///\param[in] tp The \ref monsoon::time_point "time point" of the scalar.
    ///\param[in] data The \ref monsoon::metric_value "scalar value".
    ///\param[in] is_fact Indicates if the scalar is a fact, as opposed to a speculation.
    value(time_point tp, const metric_value& data, bool is_fact)
    : tp(tp),
      data(data),
      is_fact(is_fact),
      is_emitted(false)
    {}

    ///\brief Constructor.
    ///\param[in] tp The \ref monsoon::time_point "time point" of the scalar.
    ///\param[in] data The \ref monsoon::metric_value "scalar value".
    ///\param[in] is_fact Indicates if the scalar is a fact, as opposed to a speculation.
    value(time_point tp, metric_value&& data, bool is_fact) noexcept
    : tp(tp),
      data(std::move(data)),
      is_fact(is_fact),
      is_emitted(false)
    {}

    ///\brief The \ref monsoon::time_point "time point" of the scalar.
    time_point tp;
    ///\brief The \ref monsoon::metric_value "value" of the scalar.
    metric_value data;
    ///\brief Indicates if the scalar is a fact, as opposed to a speculation.
    bool is_fact : 1;

    ///\brief Indicates if this scalar has been emitted.
    ///\details Emitted scalars still participate in interpolation.
    bool is_emitted : 1;
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
    metric_value data;
  };

  ///\brief Comparator that compares \ref monsoon::time_point "time points" of arguments.
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

  ///\brief Comparator that compares the fact property of arguments.
  ///\note True compares before false.
  class factflag_compare_ {
   public:
    template<typename X, typename Y>
    auto operator()(const X& x, const Y& y) const
    noexcept
    -> bool {
      return is_fact_(x) > is_fact_(y);
    }

   private:
    static constexpr auto is_fact_(const bool& is_fact)
    noexcept
    -> bool {
      return is_fact;
    }

    static constexpr auto is_fact_(const value& x)
    noexcept
    -> bool {
      return x.is_fact;
    }
  };

 public:
  scalar_sink(scalar_sink&&) noexcept = default;
  scalar_sink();
  ~scalar_sink() noexcept;

  /**
   * \brief Suggest the next emit time point for emit.
   *
   * \returns The time point of the oldest time point held.
   */
  auto suggest_emit_tp() const noexcept -> std::optional<time_point>;

  /**
   * \brief Mark time point as having been emitted.
   * \details Prevents suggest_emit_tp() from suggesting this time point.
   * \param[in] tp The time point to mark as emitted.
   */
  auto mark_emitted(time_point tp) noexcept -> void;

  /**
   * \brief The newest factual time point held.
   * \returns The time point of the most recent fact.
   */
  auto fact_end() const noexcept -> std::optional<time_point>;

  /**
   * \brief Retrieve value at the given time point.
   *
   * \details The value may be interpolated, if needed.
   * \param[in] tp The time point for which a value is requested.
   * \param[in] min_interp_tp The min time point which may be considered when interpolating.
   * \param[in] max_interp_tp The max time point which may be considered when interpolating.
   * \param[in] is_closed Indicates the source of values is closed.
   * \returns Value at the given time point.
   */
  auto get(time_point tp, time_point min_interp_tp, time_point max_interp_tp, bool is_closed) const -> scalar;

  /**
   * \brief Drop all speculative values before the given time point.
   */
  auto drop_speculative_before(time_point tp) -> void;

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
  [[maybe_unused, nodiscard]]
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

/**
 * \brief Vector sink.
 * \ingroup expr
 *
 * \details
 * Accepts vectors and stores them.
 * Stored vectors are used for emitting and interpolating.
 */
class vector_sink {
 public:
  vector_sink(vector_sink&&) noexcept = default;
  vector_sink(const std::shared_ptr<const match_clause>& mc);
  ~vector_sink() noexcept;

  /**
   * \brief Suggest the next emit time point for emit.
   *
   * \returns The time point of the oldest time point held.
   */
  auto suggest_emit_tp() const noexcept -> std::optional<time_point>;

  /**
   * \brief Mark time point as having been emitted.
   * \details Prevents suggest_emit_tp() from suggesting this time point.
   * \param[in] tp The time point to mark as emitted.
   */
  auto mark_emitted(time_point tp) noexcept -> void;

  /**
   * \brief The newest factual time point held.
   * \returns The time point of the most recent fact.
   */
  auto fact_end() const noexcept -> std::optional<time_point>;

  /**
   * \brief Retrieve value at the given time point.
   *
   * \details The value may be interpolated, if needed.
   * \param[in] tp The time point for which a value is requested.
   * \param[in] max_interp_tp The max time point which may be considered when interpolating.
   * \param[in] is_closed Indicates the source of values is closed.
   * \returns Value at the given time point.
   */
  auto get(time_point tp, time_point min_interp_tp, time_point max_interp_tp, bool is_closed) const -> tagged_vector;

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
  [[maybe_unused, nodiscard]]
  auto invariant() const noexcept -> bool;

 private:
  /**
   * \brief Collection of tagged scalars.
   */
  std::unordered_map<tags, scalar_sink, class match_clause::hash, match_clause::equal_to>
      data_;

  /**
   * \brief Time point of the most recently accepted fact.
   * \details We need to record this separately, in order to handle empty facts.
   */
  std::optional<time_point> last_known_fact_tp_;
};


/**
 * \brief Wrapper that connects a source and its associated sink.
 * \ingroup expr
 *
 * \details
 * Reads data from the \p Pipe (an \ref monsoon::objpipe::reader<T> "objpipe")
 * into the appropriate sink.
 *
 * \tparam Pipe An object pipe. Must be an instance of
 *   \ref monsoon::expression::scalar_objpipe "scalar_objpipe"
 *   or \ref monsoon::expression::vector_objpipe "vector_objpipe".
 *
 * To be more precise, the template relies on the value type matching.
 */
template<typename Pipe>
class pull_cycle {
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

  pull_cycle(pull_cycle&&) noexcept = default;

  /**
   * \brief Constructs a new cycle, taking values from
   * the \ref monsoon::objpipe::reader<T> "objpipe" \p source
   * and storing them into \p sink.
   */
  pull_cycle(Pipe&& source, sink_type&& sink)
  noexcept(std::is_nothrow_move_constructible_v<Pipe>
      && std::is_nothrow_move_constructible_v<sink_type>)
  : sink_(std::move(sink)),
    source_(std::move(source))
  {}

  /**
   * \brief Suggest the next emit time point for emit.
   *
   * \returns The time point of the oldest time point held.
   */
  auto suggest_emit_tp() const noexcept -> std::optional<time_point>;

  /**
   * \brief Mark time point as having been emitted.
   * \details Prevents suggest_emit_tp() from suggesting this time point.
   * \param[in] tp The time point to mark as emitted.
   */
  auto mark_emitted(time_point tp) noexcept -> void;

  /**
   * \brief Retrieve value at the given time point.
   *
   * \details Invokes
   * \code
   * sink_.get(tp, min_interp_tp, max_interp_tp, ...);
   * \endcode
   * \param tp The time point for which a value is requested.
   * \param max_interp_tp The max time point which may be considered when interpolating.
   * \returns Value at the given time point.
   */
  auto get(time_point tp, time_point min_interp_tp, time_point max_interp_tp) const
  -> decltype(std::declval<const sink_type&>()
      .get(std::declval<time_point>(), std::declval<time_point>(), std::declval<time_point>(), std::declval<bool>()));

  ///\brief Read data from source, transferring it into sink.
  ///\details Stops reading if data fails to read, or after a fact was transferred.
  ///\param[in] block If set, the read will block until data is available.
  ///\returns True if a fact was transferred.
  auto read_more(bool block = false) -> bool;

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
   * \brief Test if the underlying objpipe is pullable.
   */
  auto is_pullable() const noexcept -> bool {
    return source_.is_pullable();
  }

  /**
   * \brief Invariant.
   *
   * \details Tests if the invariant holds.
   */
  [[maybe_unused, nodiscard]]
  auto invariant() const noexcept -> bool;

 private:
  /**
   * \brief Try to read facts up-to-and-including \p tp.
   *
   * \details Repeatedly invokes read_more(), until sink_.fact_end() exceeds tp.
   * \note While this function is const, it mutates the underlying data.
   *
   * \param The time point we would like to have facts on.
   */
  auto try_forward_to_(time_point tp) const -> void;

  sink_type sink_;
  Pipe source_;
  std::optional<time_point> next_tp_;
};


template<typename PipeX, typename PipeY, typename Fn>
class pair_merger_pipe {
 public:
  using objpipe_errc = objpipe::objpipe_errc;

  using result_type = decltype(
      merger_apply(
          std::declval<const Fn&>(),
          std::declval<const pull_cycle<PipeX>&>().get(
              std::declval<time_point>(),
              std::declval<time_point>(),
              std::declval<time_point>()),
          std::declval<const pull_cycle<PipeY>&>().get(
              std::declval<time_point>(),
              std::declval<time_point>(),
              std::declval<time_point>()),
          std::declval<std::shared_ptr<const match_clause>>())
          .as_emit(std::declval<time_point>()));
  using transport_type = objpipe::detail::transport<result_type>;

  pair_merger_pipe(pair_merger_pipe&&) = default;
  template<typename FnArg>
  pair_merger_pipe(PipeX&& x, PipeY&& y,
      const std::shared_ptr<const match_clause>& mc,
      std::shared_ptr<const match_clause> out_mc,
      time_point::duration slack,
      FnArg&& fn);
  auto wait() const -> objpipe_errc;
  auto is_pullable() const noexcept -> bool;
  auto front() const -> transport_type;
  auto pop_front() -> objpipe_errc;
  auto pull() -> transport_type;
  auto try_pull() -> transport_type;

 private:
  mutable pull_cycle<PipeX> x_;
  mutable pull_cycle<PipeY> y_;
  std::shared_ptr<const match_clause> out_mc_;
  Fn fn_;
  time_point::duration slack_;
  mutable std::optional<time_point> last_front_tp_;
};


scalar_sink::scalar_sink() {}
scalar_sink::~scalar_sink() noexcept {}

auto scalar_sink::suggest_emit_tp() const
noexcept
-> std::optional<time_point> {
  assert(invariant());

  const auto emit_iter = std::find_if(data_.begin(), data_.end(),
      [](const value& v) { return !v.is_emitted; });
  if (emit_iter == data_.end()) return {};
  return emit_iter->tp;
}

auto scalar_sink::mark_emitted(time_point tp)
noexcept
-> void {
  assert(invariant());

  // Optimize for first element.
  if (!data_.empty() && data_.front().tp == tp) {
    data_.front().is_emitted = true;
  } else {
    const auto at_or_after = std::lower_bound(data_.begin(), data_.end(),
        tp, tp_compare_());
    if (at_or_after != data_.end() && at_or_after->tp == tp)
      at_or_after->is_emitted = true;
  }

  assert(invariant());
}

auto scalar_sink::fact_end() const
noexcept
-> std::optional<time_point> {
  assert(invariant());

  const auto first_speculation = std::upper_bound(
      data_.begin(), data_.end(),
      true,
      factflag_compare_());
  if (first_speculation == data_.begin())
    return {};
  return std::prev(first_speculation)->tp;
}

auto scalar_sink::get(time_point tp,
    time_point min_interp_tp,
    time_point max_interp_tp,
    bool is_closed) const
-> scalar {
  assert(min_interp_tp <= tp && tp <= max_interp_tp);
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
    if (!recent_.has_value() || recent_->tp < min_interp_tp)
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

    if (predecessor->tp < min_interp_tp)
      return scalar::absent(at_or_successor->is_fact);
    return scalar(
        interpolate(tp,
            { predecessor->tp, predecessor->data },
            { at_or_successor->tp, at_or_successor->data }),
        at_or_successor->is_fact);
  }
  // unreachable
}

auto scalar_sink::drop_speculative_before(time_point tp) -> void {
  assert(invariant());

  auto tp_begin = std::lower_bound(data_.begin(), data_.end(),
      tp,
      tp_compare_());
  if (tp_begin != data_.end() && tp_begin->is_fact) return;

  auto spec_begin = std::lower_bound(data_.begin(), tp_begin,
      false,
      factflag_compare_());
  data_.erase(spec_begin, tp_begin);

  assert(invariant());
  assert(std::lower_bound(data_.begin(), data_.end(), false, factflag_compare_()) == data_.end()
      || std::lower_bound(data_.begin(), data_.end(), false, factflag_compare_())->tp >= tp);
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
    const auto first_speculative = std::lower_bound(
        data_.begin(), keep_begin,
        false,
        factflag_compare_());
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
  if (at_or_successor == data_.end()) {
    data_.emplace_back(tp, std::move(mv), is_fact);
    // Potentially invalidates at_or_successor.
    ins_pos = data_.end();
  } else if (at_or_successor->tp == tp) {
    assert(!at_or_successor->is_fact);
    at_or_successor->data = std::move(mv);
    at_or_successor->is_fact = is_fact;
    at_or_successor->is_emitted = false;

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
        factflag_compare_());
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
      factflag_compare_());

  return
      std::all_of(
          data_.begin(), speculative_begin,
          [](const value& v) { return v.is_fact; })
      && std::none_of(
          speculative_begin, data_.end(),
          [](const value& v) { return v.is_fact; })
      && std::is_sorted(
          data_.begin(), data_.end(),
          tp_compare_());
}


vector_sink::vector_sink(const std::shared_ptr<const match_clause>& mc)
: data_(0, mc, mc)
{}

vector_sink::~vector_sink() noexcept {}

auto vector_sink::suggest_emit_tp() const
noexcept
-> std::optional<time_point> {
  assert(invariant());

  std::optional<time_point> min = objpipe::of(std::cref(data_))
      .iterate()
      .select_second()
      .transform(&scalar_sink::suggest_emit_tp)
      .filter([](const std::optional<time_point>& v) { return v.has_value(); })
      .deref()
      .min();

  if (!min.has_value())
    min = last_known_fact_tp_;
  return min;
}

auto vector_sink::mark_emitted(time_point tp)
noexcept
-> void {
  assert(invariant());

  std::for_each(data_.begin(), data_.end(),
      [tp](auto& data_elem) {
        data_elem.second.mark_emitted(tp);
      });

  assert(invariant());
}

auto vector_sink::fact_end() const
noexcept
-> std::optional<time_point> {
  assert(invariant());

  return last_known_fact_tp_;
}

auto vector_sink::get(time_point tp,
    time_point min_interp_tp,
    time_point max_interp_tp,
    bool is_closed) const
-> tagged_vector {
  assert(min_interp_tp <= tp && tp <= max_interp_tp);
  assert(invariant());

  tagged_vector result = tagged_vector(
      data_.bucket_count(),
      data_.hash_function(),
      data_.key_eq(),
      true);

  std::for_each(
      data_.begin(), data_.end(),
      [this, &result, tp, min_interp_tp, max_interp_tp, is_closed](const auto& elem) {
        const auto& t = elem.first;
        auto s = elem.second.get(tp, min_interp_tp, max_interp_tp, is_closed);

        // Only mark result as speculative, if the value is speculative.
        //
        // The scalar_sink is unaware of absent values (because scalars
        // can never be absent) and thus we need to verify the speculation-flag
        // against last_known_fact_tp_.
        if (!s.is_fact && last_known_fact_tp_ < max_interp_tp)
          result.is_fact = false;

        if (s.value.has_value())
          result.values.emplace(t, *std::move(s.value));
      });

  // When max_interp_tp is known, everything is a fact.
  assert(last_known_fact_tp_ < max_interp_tp || result.is_fact);
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
  assert(last_known_fact_tp_ < tp);

  bool accepted = std::visit(
      overload(
          [this, &tp](expression::speculative_vector&& v) {
            return data_[std::get<0>(std::move(v))]
                .accept(expression::scalar_emit_type(tp, std::in_place_index<0>, std::get<1>(std::move(v))));
          },
          [this, &tp](expression::factual_vector&& v) {
#if __cplusplus >= 201703
            while (!v.empty()) {
              auto nh = v.extract(v.begin());
              [[maybe_unused]] const bool accepted = data_[std::move(nh.key())]
                  .accept(expression::scalar_emit_type(tp, std::in_place_index<1>, std::move(nh.mapped())));
              assert(accepted);
            }
#else
            for (auto& item : v) {
              [[maybe_unused]] const bool accepted = data_[item.first]
                  .accept(expression::scalar_emit_type(tp, std::in_place_index<1>, std::move(item.second)));
              assert(accepted);
            }
#endif

            // Remove speculative records preceding this fact.
            auto data_iter = data_.begin();
            while (data_iter != data_.end()) {
              data_iter->second.drop_speculative_before(tp);
              if (data_iter->second.empty())
                data_iter = data_.erase(data_iter);
              else
                ++data_iter;
            }

            last_known_fact_tp_.emplace(tp);
            return true;
          }),
      std::move(emt.data));

  assert(invariant());
  return accepted;
}

auto vector_sink::invariant() const
noexcept
-> bool {
  const std::optional<time_point> last_scalar_fact = objpipe::of(std::cref(data_))
      .iterate()
      .select_second()
      .transform(&scalar_sink::fact_end)
      .filter([](const std::optional<time_point>& v) { return v.has_value(); })
      .deref()
      .max();

  return last_known_fact_tp_ >= last_scalar_fact
      && (objpipe::of(std::ref(data_))
          .iterate()
          .select_second()
          .transform(&scalar_sink::empty)
          .transform(std::logical_not<bool>())
          .reduce(std::logical_and<bool>())
          .value_or(true));
}


template<typename Pipe>
auto pull_cycle<Pipe>::suggest_emit_tp() const noexcept
-> std::optional<time_point> {
  assert(invariant());

  return next_tp_;
}

template<typename Pipe>
auto pull_cycle<Pipe>::mark_emitted(time_point tp)
noexcept
-> void {
  assert(invariant());

  sink_.mark_emitted(tp);
  next_tp_ = sink_.suggest_emit_tp();

  assert(invariant());
}

template<typename Pipe>
auto pull_cycle<Pipe>::get(time_point tp, time_point min_interp_tp, time_point max_interp_tp) const
-> decltype(std::declval<const sink_type&>()
    .get(std::declval<time_point>(), std::declval<time_point>(), std::declval<time_point>(), std::declval<bool>())) {
  assert(invariant());

  // Read up to and including max_interp_tp,
  // to favour facts that are known correct
  // (as opposed to using facts that may change
  // due to interpolation and thus have to be labelled
  // speculative).
  try_forward_to_(max_interp_tp);
  return sink_.get(tp, min_interp_tp, max_interp_tp, !source_.is_pullable());
}

template<typename Pipe>
auto pull_cycle<Pipe>::read_more(bool block)
-> bool {
  assert(invariant());

  if (!source_.is_pullable()) return false;
  bool accepted_fact = false;

  do {
    std::optional<typename Pipe::value_type> next;

    if (!next_tp_.has_value() && block) {
      try {
        next = source_.pull();
      } catch (const std::system_error& e) {
        if (e.code() != objpipe::objpipe_errc::closed)
          throw;
      }
    } else {
      next = source_.try_pull();
    }
    if (!next.has_value()) break;

    const bool is_fact = (next->data.index() == 1);
    const time_point tp = next->tp;

    const bool accepted = sink_.accept(*std::move(next));
    accepted_fact = (accepted && is_fact);

    if (accepted_fact && (!next_tp_.has_value() || *next_tp_ < tp))
      next_tp_ = sink_.suggest_emit_tp();

    assert(invariant());
  } while (!accepted_fact);

  assert(invariant());
  return accepted_fact;
}

template<typename Pipe>
auto pull_cycle<Pipe>::try_forward_to_(time_point tp) const
-> void {
  assert(invariant());

  bool failed_to_read = !source_.is_pullable();
  while (!failed_to_read && sink_.fact_end() < tp) {
    // We hide that we're actually changing data underneath,
    // since this is the only function that requires it.
    //
    // It's semantically const, in that it does not affect the
    // logical view of the data.
    failed_to_read = !const_cast<pull_cycle&>(*this).read_more();
  }

  assert(invariant());
}

template<typename Pipe>
auto pull_cycle<Pipe>::invariant() const
noexcept
-> bool {
  return next_tp_ == sink_.suggest_emit_tp();
}

template<typename Pipe>
auto pull_cycle<Pipe>::forward_to_time(time_point tp, time_point expire_before)
-> void {
  sink_.forward_to_time(tp, expire_before);
}


template<typename Pipe>
auto create_sink_for([[maybe_unused]] const std::shared_ptr<const match_clause>& mc)
-> std::enable_if_t<
    std::is_same_v<expression::scalar_emit_type, typename Pipe::value_type>,
    scalar_sink> {
  return scalar_sink();
}

template<typename Pipe>
auto create_sink_for(std::shared_ptr<const match_clause> mc)
-> std::enable_if_t<
    std::is_same_v<expression::vector_emit_type, typename Pipe::value_type>,
    vector_sink> {
  assert(mc != nullptr);
  return vector_sink(std::move(mc));
}


template<typename PipeX, typename PipeY, typename Fn>
template<typename FnArg>
pair_merger_pipe<PipeX, PipeY, Fn>::pair_merger_pipe(PipeX&& x, PipeY&& y,
    const std::shared_ptr<const match_clause>& mc,
    std::shared_ptr<const match_clause> out_mc,
    time_point::duration slack,
    FnArg&& fn)
: x_(std::move(x), create_sink_for<PipeX>(mc)),
  y_(std::move(y), create_sink_for<PipeY>(mc)),
  out_mc_(std::move(out_mc)),
  fn_(std::forward<FnArg>(fn)),
  slack_(slack)
{}

template<typename PipeX, typename PipeY, typename Fn>
auto pair_merger_pipe<PipeX, PipeY, Fn>::wait() const -> objpipe_errc {
  assert(!last_front_tp_.has_value());

  std::optional<time_point> x_tp = x_.suggest_emit_tp();
  if (!x_tp.has_value()) {
    x_.read_more(true);
    x_tp = x_.suggest_emit_tp();
  }
  assert(x_tp.has_value() || !x_.is_pullable());

  std::optional<time_point> y_tp = y_.suggest_emit_tp();
  if (!y_tp.has_value()) {
    y_.read_more(true);
    y_tp = y_.suggest_emit_tp();
  }
  assert(y_tp.has_value() || !y_.is_pullable());

  if (x_tp.has_value() && y_tp.has_value())
    return objpipe_errc::success;
  else
    return objpipe_errc::closed;
}

template<typename PipeX, typename PipeY, typename Fn>
auto pair_merger_pipe<PipeX, PipeY, Fn>::is_pullable() const
noexcept
-> bool {
  return (x_.is_pullable() || x_.suggest_emit_tp().has_value())
      && (y_.is_pullable() || y_.suggest_emit_tp().has_value());
}

template<typename PipeX, typename PipeY, typename Fn>
auto pair_merger_pipe<PipeX, PipeY, Fn>::front() const
-> transport_type {
  assert(!last_front_tp_.has_value());

  std::optional<time_point> x_tp = x_.suggest_emit_tp();
  if (!x_tp.has_value()) {
    x_.read_more(true);
    x_tp = x_.suggest_emit_tp();
  }
  assert(x_tp.has_value() || !x_.is_pullable());

  std::optional<time_point> y_tp = y_.suggest_emit_tp();
  if (!y_tp.has_value()) {
    y_.read_more(true);
    y_tp = y_.suggest_emit_tp();
  }
  assert(y_tp.has_value() || !y_.is_pullable());

  if (!x_tp.has_value() || !y_tp.has_value())
    return transport_type(std::in_place_index<1>, objpipe_errc::closed);

  const time_point tp = std::min(*x_tp, *y_tp);
  last_front_tp_.emplace(tp);

  auto result = merger_apply(
      fn_,
      x_.get(tp, tp - slack_, tp + slack_),
      y_.get(tp, tp - slack_, tp + slack_),
      out_mc_);
  if (result.is_fact) { // Factual emit
    x_.forward_to_time(tp, tp - slack_);
    y_.forward_to_time(tp, tp - slack_);
  }

  return transport_type(std::in_place_index<0>, std::move(result).as_emit(tp));
}

template<typename PipeX, typename PipeY, typename Fn>
auto pair_merger_pipe<PipeX, PipeY, Fn>::pop_front()
-> objpipe_errc {
  if (!last_front_tp_.has_value()) {
    transport_type front_result = front();
    if (front_result.errc() != objpipe_errc::success)
      return front_result.errc();
  }
  assert(last_front_tp_.has_value());

  x_.mark_emitted(*last_front_tp_);
  y_.mark_emitted(*last_front_tp_);
  last_front_tp_.reset();
  return objpipe_errc::success;
}

template<typename PipeX, typename PipeY, typename Fn>
auto pair_merger_pipe<PipeX, PipeY, Fn>::pull()
-> transport_type {
  assert(!last_front_tp_.has_value());

  std::optional<time_point> x_tp = x_.suggest_emit_tp();
  if (!x_tp.has_value()) {
    x_.read_more(true);
    x_tp = x_.suggest_emit_tp();
  }
  assert(x_tp.has_value() || !x_.is_pullable());

  std::optional<time_point> y_tp = y_.suggest_emit_tp();
  if (!y_tp.has_value()) {
    y_.read_more(true);
    y_tp = y_.suggest_emit_tp();
  }
  assert(y_tp.has_value() || !y_.is_pullable());

  if (!x_tp.has_value() || !y_tp.has_value())
    return transport_type(std::in_place_index<1>, objpipe_errc::closed);

  const time_point tp = std::min(*x_tp, *y_tp);
  auto result = merger_apply(
      fn_,
      x_.get(tp, tp - slack_, tp + slack_),
      y_.get(tp, tp - slack_, tp + slack_),
      out_mc_);
  if (result.is_fact) { // Factual emit
    x_.forward_to_time(tp, tp - slack_);
    y_.forward_to_time(tp, tp - slack_);
  }

  x_.mark_emitted(tp);
  y_.mark_emitted(tp);
  return transport_type(std::in_place_index<0>, std::move(result).as_emit(tp));
}

template<typename PipeX, typename PipeY, typename Fn>
auto pair_merger_pipe<PipeX, PipeY, Fn>::try_pull()
-> transport_type {
  assert(!last_front_tp_.has_value());

  std::optional<time_point> x_tp = x_.suggest_emit_tp();
  if (!x_tp.has_value()) {
    x_.read_more(false);
    x_tp = x_.suggest_emit_tp();
  }

  std::optional<time_point> y_tp = y_.suggest_emit_tp();
  if (!y_tp.has_value()) {
    y_.read_more(false);
    y_tp = y_.suggest_emit_tp();
  }

  if (!x_tp.has_value() || !y_tp.has_value()) {
    if (!x_.is_pullable() && !y_.is_pullable())
      return transport_type(std::in_place_index<1>, objpipe_errc::closed);
    else
      return transport_type(std::in_place_index<1>, objpipe_errc::success);
  }

  const time_point tp = std::min(*x_tp, *y_tp);
  auto result = merger_apply(
      fn_,
      x_.get(tp, tp - slack_, tp + slack_),
      y_.get(tp, tp - slack_, tp + slack_),
      out_mc_);
  if (result.is_fact) { // Factual emit
    x_.forward_to_time(tp, tp - slack_);
    y_.forward_to_time(tp, tp - slack_);
  }

  x_.mark_emitted(tp);
  y_.mark_emitted(tp);
  return transport_type(std::in_place_index<0>, std::move(result).as_emit(tp));
}


} /* namespace monsoon::expressions::<unnamed> */


auto make_merger(metric_value(*fn)(const metric_value&, const metric_value&),
    std::shared_ptr<const match_clause> mc,
    std::shared_ptr<const match_clause> out_mc,
    time_point::duration slack,
    expression::scalar_objpipe&& x,
    expression::scalar_objpipe&& y)
-> expression::scalar_objpipe {
  using impl_type = pair_merger_pipe<
      expression::scalar_objpipe,
      expression::scalar_objpipe,
      metric_value(*)(const metric_value&, const metric_value&)>;

  return objpipe::detail::adapter(impl_type(std::move(x), std::move(y), mc, out_mc, slack, fn))
      .iterate();
}

auto make_merger(metric_value(*fn)(const metric_value&, const metric_value&),
    std::shared_ptr<const match_clause> mc,
    std::shared_ptr<const match_clause> out_mc,
    time_point::duration slack,
    expression::vector_objpipe&& x,
    expression::scalar_objpipe&& y)
-> expression::vector_objpipe {
  using impl_type = pair_merger_pipe<
      expression::vector_objpipe,
      expression::scalar_objpipe,
      metric_value(*)(const metric_value&, const metric_value&)>;

  return objpipe::detail::adapter(impl_type(std::move(x), std::move(y), mc, out_mc, slack, fn))
      .iterate();
}

auto make_merger(metric_value(*fn)(const metric_value&, const metric_value&),
    std::shared_ptr<const match_clause> mc,
    std::shared_ptr<const match_clause> out_mc,
    time_point::duration slack,
    expression::scalar_objpipe&& x,
    expression::vector_objpipe&& y)
-> expression::vector_objpipe {
  using impl_type = pair_merger_pipe<
      expression::scalar_objpipe,
      expression::vector_objpipe,
      metric_value(*)(const metric_value&, const metric_value&)>;

  return objpipe::detail::adapter(impl_type(std::move(x), std::move(y), mc, out_mc, slack, fn))
      .iterate();
}

auto make_merger(metric_value(*fn)(const metric_value&, const metric_value&),
    std::shared_ptr<const match_clause> mc,
    std::shared_ptr<const match_clause> out_mc,
    time_point::duration slack,
    expression::vector_objpipe&& x,
    expression::vector_objpipe&& y)
-> expression::vector_objpipe {
  using impl_type = pair_merger_pipe<
      expression::vector_objpipe,
      expression::vector_objpipe,
      metric_value(*)(const metric_value&, const metric_value&)>;

  return objpipe::detail::adapter(impl_type(std::move(x), std::move(y), mc, out_mc, slack, fn))
      .iterate();
}


} /* namespace monsoon::expressions */
