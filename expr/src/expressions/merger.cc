///\file
///\ingroup expr

#include <monsoon/expressions/merger.h>
#include <algorithm>
#include <deque>
#include <functional>
#include <iterator>
#include <map>
#include <numeric>
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
#include <objpipe/of.h>
#include <monsoon/expr_export_.h>

namespace monsoon::expressions {
namespace {


/**
 * \brief Time point selector for mergers.
 * \ingroup expr
 * \details Mergers need both speculative and factual emit timepoints.
 */
class time_point_selector {
 public:
  ///\brief Create an empty time_point_selector.
  constexpr time_point_selector() noexcept = default;

  constexpr time_point_selector(
      const std::optional<time_point>& speculative,
      const std::optional<time_point>& factual)
  noexcept
  : speculative(speculative),
    factual(factual)
  {}

  ///\brief Retrieve the best suggestion of a time point emit.
  constexpr auto get() const
  noexcept
  -> const std::optional<time_point>& {
    return (!factual.has_value() && speculative.has_value()
        ? speculative
        : factual);
  }

  std::optional<time_point> speculative, factual;
};

///\brief Compute the minimum between two optional time points, ignoring empty optionals.
///\ingroup expr
constexpr auto min_of_present_opts(
    const std::optional<time_point>& x,
    const std::optional<time_point>& y)
noexcept
-> const std::optional<time_point>& {
  if (!y.has_value()) return x;
  if (!x.has_value() || *x > *y) return y;
  return x;
}

///\brief Compute the minimum between two \ref time_point_selector "time point selectors".
///\ingroup expr
///\relates time_point_selector
constexpr auto tps_min(const time_point_selector& x, const time_point_selector& y)
noexcept
-> time_point_selector {
  return time_point_selector(
      min_of_present_opts(x.speculative, y.speculative),
      std::min(x.factual, y.factual));
}

///\brief Equality comparison.
///\relates time_point_selector
constexpr auto operator==(const time_point_selector& x, const time_point_selector& y)
noexcept
-> bool {
  return x.factual == y.factual && x.speculative == y.speculative;
}

///\brief Inequality comparison.
///\relates time_point_selector
constexpr auto operator!=(const time_point_selector& x, const time_point_selector& y)
noexcept
-> bool {
  return !(x == y);
}


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
  -> std::optional<expression::scalar_emit_type> {
    using scalar_emit_type = expression::scalar_emit_type;
    if (!value.has_value()) return {};

    if (is_fact)
      return scalar_emit_type(tp, std::in_place_index<1>, *std::move(value));
    else
      return scalar_emit_type(tp, std::in_place_index<0>, *std::move(value));
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

    ///\brief Indicates if this scalar has been emitted as a speculation.
    ///\details Emitted scalars still participate in interpolation.
    ///\note Facts are always erased using forward_to_time.
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
  auto suggest_emit_tp() const noexcept -> time_point_selector;

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
  auto suggest_emit_tp() const noexcept -> time_point_selector;

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
  bool last_known_fact_emitted_ = false;
};


/**
 * \brief Wrapper that connects a source and its associated sink.
 * \ingroup expr
 *
 * \details
 * Reads data from the \p Pipe (an \ref objpipe::reader<T> "objpipe")
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
   * the \ref objpipe::reader<T> "objpipe" \p source
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
  auto suggest_emit_tp() const noexcept -> time_point_selector;

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

  template<bool Enable = objpipe::detail::adapt::has_ioc_push<objpipe::detail::adapter_underlying_type_t<Pipe>, objpipe::existingthread_push>>
  auto can_push(const objpipe::existingthread_push& policy) const
  noexcept
  -> std::enable_if_t<Enable, bool> {
    return source_.underlying().can_push(policy);
  }

  template<typename Acceptor, bool Enable = objpipe::detail::adapt::has_ioc_push<Pipe, objpipe::existingthread_push>>
  auto ioc_push(const objpipe::existingthread_push& policy, Acceptor&& acceptor)
  -> void {
    source_.underlying().ioc_push(policy, std::forward<Acceptor>(acceptor));
  }

  auto release_source() &&
  noexcept
  -> Pipe&& {
    return std::move(source_);
  }

  /**
   * \brief Invariant.
   *
   * \details Tests if the invariant holds.
   */
  [[maybe_unused, nodiscard]]
  auto invariant() const noexcept -> bool;

 private:
  sink_type sink_;
  mutable Pipe source_;
  time_point_selector next_tp_;
};


template<typename Pipe, typename MergerImpl>
class push_cycle
: public pull_cycle<Pipe>::sink_type
{
 public:
  using sink_type = typename pull_cycle<Pipe>::sink_type;

  class acceptor {
   private:
    using objpipe_errc = objpipe::objpipe_errc;

   public:
    acceptor(acceptor&&) noexcept = default;

    acceptor(std::shared_ptr<push_cycle> ptr)
    : ptr_(std::move(ptr))
    {}

    ~acceptor() noexcept {
      const std::lock_guard<std::mutex> lck{ ptr_->mtx_() };
      ptr_->has_writer_ = false;
    }

    template<typename T>
    auto operator()(T&& next)
    -> std::enable_if_t<
        std::is_same_v<typename Pipe::value_type, std::remove_cv_t<std::remove_reference_t<T>>>,
        objpipe_errc> {
      const bool is_fact = (next.data.index() == 1);
      const time_point tp = next.tp;

      const std::unique_lock<std::mutex> lck{ ptr_->mtx_() };
      if (ptr_->is_closed_(lck)) return objpipe_errc::closed;
      const bool accepted = ptr_->accept(std::forward<T>(next));

      if (accepted) {
        auto& next_tp_ = ptr_->next_tp_;
        bool next_tp_changed = false;
        if (is_fact && !next_tp_.factual.has_value()) {
          next_tp_.factual = tp;
          next_tp_changed = true;
        }
        if (!next_tp_.speculative.has_value() || *next_tp_.speculative > tp) {
          next_tp_.speculative = tp;
          next_tp_changed = true;
        }
        if (next_tp_changed) ptr_->maybe_emit_(lck);
      }

      return objpipe_errc::success;
    }

    auto push_exception(std::exception_ptr exptr)
    noexcept
    -> void {
      ptr_->push_exception_(std::move(exptr));
    }

   private:
    std::shared_ptr<push_cycle> ptr_;
  };

  push_cycle(sink_type&& sink, MergerImpl& merger_impl)
  noexcept(std::is_nothrow_move_constructible_v<sink_type>)
  : sink_type(std::move(sink)),
    merger_impl_(merger_impl)
  {}

  push_cycle(push_cycle&&) = default;

  auto suggest_emit_tp() const noexcept -> time_point_selector {
    assert(next_tp_ == this->sink_type::suggest_emit_tp());
    return next_tp_;
  }

  auto start(
      std::shared_ptr<void> owner,
      Pipe&& pipe,
      const objpipe::existingthread_push& policy)
  -> void {
    std::move(pipe)
        .underlying()
        .ioc_push(policy, acceptor(std::shared_ptr<push_cycle>(owner, this)));
  }

  auto get(time_point tp, time_point tp_min_interp, time_point tp_max_interp) {
    return this->sink_type::get(tp, tp_min_interp, tp_max_interp, !has_writer_);
  }

 private:
  auto mtx_() const
  noexcept
  -> std::mutex& {
    return merger_impl_.mtx;
  }

  auto push_exception_(std::exception_ptr exptr)
  noexcept
  -> void {
    merger_impl_.push_exception(std::move(exptr));
  }

  auto maybe_emit_(const std::unique_lock<std::mutex>& lck)
  -> void {
    merger_impl_.maybe_emit(lck);
  }

  auto is_closed_(const std::unique_lock<std::mutex>& lck)
  -> bool {
    return merger_impl_.is_closed(lck);
  }

  time_point_selector next_tp_;
  MergerImpl& merger_impl_;
  bool has_writer_ = true;
};


template<typename T>
auto suggest_emit_tp_(pull_cycle<T>& cycle, bool block)
-> time_point_selector {
  time_point_selector suggestion = cycle.suggest_emit_tp();
  if (!suggestion.factual.has_value()) {
    cycle.read_more(block);
    suggestion = cycle.suggest_emit_tp();
  }
  return suggestion;
}

template<typename T, typename O>
auto suggest_emit_tp_(const push_cycle<T, O>& cycle, [[maybe_unused]] bool block)
-> time_point_selector {
  return cycle.suggest_emit_tp();
}

template<typename... T>
auto suggest_emit_tp_(std::variant<T...>& cycle, bool block)
-> time_point_selector {
  return std::visit(
      [block](auto& pipe) {
        return suggest_emit_tp_(pipe, block);
      },
      cycle);
}

template<typename... T>
auto suggest_emit_tp_(const std::variant<T...>& cycle, bool block)
-> time_point_selector {
  return std::visit(
      [block](auto& pipe) {
        return suggest_emit_tp_(pipe, block);
      },
      cycle);
}

/**
 * \brief Algorithm used by mergers to select a time point for emit.
 * \ingroup expr
 * \details Reduction on time_point_selector, over a collection of pipes.
 */
template<typename PipesCollection>
auto suggest_emit_tp(PipesCollection& pipes, bool block)
-> std::tuple<time_point, bool> {
  time_point_selector selector;

#if __cplusplus >= 201703
  selector = std::reduce(
      pipes.begin(),
      pipes.end(),
      time_point_selector(),
      [block](const time_point_selector& selector, auto& pipe) {
        return tps_min(selector, suggest_emit_tp_(pipe, block));
      });
#else
  selector = std::accumulate(
      pipes.begin(),
      pipes.end(),
      time_point_selector(),
      [block](const time_point_selector& selector, auto& pipe) {
        return tps_min(selector, suggest_emit_tp_(pipe, block));
      });
#endif

  std::optional<time_point> opt_tp = selector.get();
  if (opt_tp.has_value())
    return { *opt_tp, true };
  else
    return { time_point(), false };
}

template<typename InputCollection>
auto compute_scalar_emit(
    time_point tp,
    InputCollection& pipes,
    const std::function<metric_value(const std::vector<metric_value>&)>& fn,
    time_point::duration slack) {
  merger_apply_scalar result(nullptr, true);

  // Create argument set for function invocation.
  // Also fill in result.is_fact, which is true iff all of the values are a fact.
  std::vector<metric_value> args;
  args.reserve(pipes.size());
  std::for_each(
      pipes.begin(),
      pipes.end(),
      [&result, tp, &args, slack](auto& x) {
        scalar s = x.get(tp, tp - slack, tp + slack);
        result.is_fact &= s.is_fact;
        if (s.value.has_value())
          args.push_back(std::move(*s.value));
      });

  // Invoke computation.
  // Note that if any of the metric values is absent,
  // the computation is skipped and an absent value is placed
  // instead.
  // (We detect this using the observation that the argument set is
  // shorter than the input set.)
  if (args.size() == pipes.size()) result.value = fn(args);

  if (result.is_fact) {
    // Drop everything at/before tp.
    std::for_each(
        pipes.begin(),
        pipes.end(),
        [tp, slack](auto& x) {
          x.forward_to_time(tp, tp - slack);
        });
  } else {
    // Mark time point as emitted.
    std::for_each(
        pipes.begin(),
        pipes.end(),
        [tp](auto& x) {
          x.mark_emitted(tp);
        });
  }

  return std::move(result).as_emit(tp);
}

auto compute_vector_emit_apply_(
    const std::function<metric_value(const std::vector<metric_value>&)>& fn,
    const match_clause& mc,
    merger_apply_vector::map_type& out,
    const std::vector<std::variant<metric_value, tagged_vector::map_type>>& args)
-> void {
  struct compute_vector_emit_apply_ctx_ {
    const std::function<metric_value(const std::vector<metric_value>&)>& fn;
    const match_clause& mc;
    merger_apply_vector::map_type& result;
    const std::vector<std::variant<metric_value, tagged_vector::map_type>>& args;
    std::vector<metric_value> fn_args;

    auto operator()(
        const std::optional<tags>& tag_set,
        std::vector<metric_value>::iterator out_iter,
        std::vector<std::variant<metric_value, tagged_vector::map_type>>::const_iterator in_iter)
    -> void {
      // Equal position assertion.
      assert((out_iter == fn_args.end()) == (in_iter == args.end()));

      // Sentinel operation.
      if (out_iter == fn_args.end()) {
        assert(tag_set.has_value());

        // Add computed value to result set.
        merger_apply_vector::map_type::iterator pos;
        bool is_new_value;
        std::tie(pos, is_new_value) = result.emplace(*tag_set, fn(fn_args));

        // Invalidate collisions by setting them to empty.
        if (!is_new_value) pos->second = metric_value();
        return;
      }

      // Recurse, with current values filled in.
      std::visit(
          overload(
              [&](const metric_value& v) {
                *out_iter = v;
                ++out_iter;
                ++in_iter;

                (*this)(
                    tag_set,
                    std::move(out_iter),
                    std::move(in_iter));
              },
              [&](const tagged_vector::map_type& m) {
                if (tag_set.has_value()) {
                  auto elem_iter = m.find(*tag_set);
                  if (elem_iter == m.end()) return;
                  *out_iter = elem_iter->second;
                  ++out_iter;
                  ++in_iter;

                  (*this)(
                      mc.reduce(*tag_set, elem_iter->first),
                      std::move(out_iter),
                      std::move(in_iter));
                } else {
                  std::for_each(
                      m.begin(),
                      m.end(),
                      [&, in_iter, out_iter](const auto& pair) {
                        *out_iter = pair.second;

                        (*this)(
                            pair.first,
                            std::next(out_iter),
                            std::next(in_iter));
                      });
                }
              }),
          *in_iter);
    }
  };

  compute_vector_emit_apply_ctx_ ctx{ fn, mc, out, args, std::vector<metric_value>() };
  ctx.fn_args.resize(args.size());

  ctx({}, ctx.fn_args.begin(), ctx.args.begin());
}

template<typename InputCollection>
auto compute_vector_emit(
    time_point tp,
    InputCollection& pipes,
    const std::function<metric_value(const std::vector<metric_value>&)>& fn,
    const match_clause& mc,
    time_point::duration slack,
    const std::shared_ptr<const match_clause>& out_mc) {
  using args_vector = std::vector<std::variant<metric_value, tagged_vector::map_type>>;

  // Create argument set for function invocation.
  // Also fill in result.is_fact, which is true iff all of the values are a fact.
  args_vector args;
  args.reserve(pipes.size());
  bool is_fact = true;
  merger_apply_vector::map_type::size_type bucket_count = 1;
  std::for_each(
      pipes.begin(),
      pipes.end(),
      [&](auto& x) {
          std::variant<scalar, tagged_vector> scalar_or_vector = std::visit(
              [tp, slack](auto& x) -> std::variant<scalar, tagged_vector> {
                return x.get(tp, tp - slack, tp + slack);
              },
              x);

          std::visit(
              overload(
                  [&is_fact, tp, &args, slack](scalar&& s) {
                    is_fact &= s.is_fact;
                    if (s.value.has_value())
                      args.push_back(std::move(*s.value));
                  },
                  [&bucket_count, &is_fact, tp, &args, slack](tagged_vector&& v) {
                    is_fact &= v.is_fact;
                    bucket_count = std::max(bucket_count, v.values.bucket_count());
                    args.push_back(std::move(v.values));
                  }),
              std::move(scalar_or_vector));
      });

  merger_apply_vector result(bucket_count, out_mc, out_mc, is_fact);

  // Invoke computation.
  // Note that if any of the metric values is absent,
  // the computation is skipped and an absent value is placed
  // instead.
  // (We detect this using the observation that the argument set is
  // shorter than the input set.)
  if (args.size() == pipes.size())
    compute_vector_emit_apply_(fn, mc, result.values, args);

  if (result.is_fact) {
    // Drop everything at/before tp.
    std::for_each(
        pipes.begin(),
        pipes.end(),
        [tp, slack](auto& x) {
          std::visit(
              [&](auto& x) {
                x.forward_to_time(tp, tp - slack);
              },
              x);
        });
  } else {
    // Mark time point as emitted.
    std::for_each(
        pipes.begin(),
        pipes.end(),
        [tp](auto& x) {
          std::visit(
              [&](auto& x) {
                x.mark_emitted(tp);
              },
              x);
        });
  }

  return std::move(result).as_emit(tp);
}


template<typename Acceptor>
class scalar_merger_push
: public std::enable_shared_from_this<scalar_merger_push<Acceptor>>
{
 private:
  using scalar_objpipe = expression::scalar_objpipe;

 public:
  using fn_type = std::function<metric_value(const std::vector<metric_value>&)>;

  scalar_merger_push(
      Acceptor&& acceptor,
      time_point::duration slack,
      fn_type fn)
  : fn_(std::move(fn)),
    slack_(slack),
    acceptor_(std::move(acceptor))
  {}

  scalar_merger_push(scalar_merger_push&&) = delete;

  ~scalar_merger_push() noexcept {
    try {
      std::unique_lock<std::mutex> lck{ mtx };
      maybe_emit(lck, true);
    } catch (...) {
      acceptor_.push_exception(std::current_exception());
    }
  }

  auto start(std::vector<scalar_objpipe>&& inputs, const objpipe::existingthread_push& policy)
  -> void {
    // First, create all the sinks.
    inputs_.reserve(inputs.size());
    std::generate_n(
        std::back_inserter(inputs_), inputs.size(),
        [this]() -> push_cycle<scalar_objpipe, scalar_merger_push> {
          using sink_type = typename push_cycle<scalar_objpipe, scalar_merger_push>::sink_type;
          return push_cycle<scalar_objpipe, scalar_merger_push>(sink_type(), *this);
        });

    // Next, start each sink.
    // We do this in a separate step, to easier guarantee that
    // the inputs_ vector won't reallocate.
    // (That may not happen, as it would invalidate pointers.)
    auto task_iter = inputs_.begin();
    for (scalar_objpipe& pipe : inputs) {
      assert(task_iter != inputs_.end());
      task_iter->start(this->shared_from_this(), std::move(pipe), policy);

      ++task_iter;
    }
    assert(task_iter == inputs_.end());
  }

  auto push_exception(std::exception_ptr exptr)
  noexcept
  -> void {
    std::lock_guard<std::mutex> lck{ mtx };
    closed_ = true;
    acceptor_.push_exception(std::move(exptr));
  }

  auto is_closed([[maybe_unused]] const std::unique_lock<std::mutex>& lck) const
  noexcept
  -> bool {
    assert(lck.owns_lock() && lck.mutex() == &mtx);
    return closed_;
  }

  auto maybe_emit([[maybe_unused]] const std::unique_lock<std::mutex>& lck, bool closed = false)
  -> void {
    assert(lck.owns_lock() && lck.mutex() == &mtx);

    while (!this->closed_) {
      // Compute minimum time point.
      // If all of the inputs fail to supply a time point,
      // we're in need of more data.
      time_point tp;
      {
        bool has_suggestion;
        std::tie(tp, has_suggestion) = suggest_emit_tp(inputs_, true);

        if (!has_suggestion)
          return;
      }

      objpipe::objpipe_errc e = acceptor_(compute_scalar_emit(tp, inputs_, fn_, slack_));
      if (e != objpipe::objpipe_errc::success) closed_ = true;
    }
  }

  std::mutex mtx;

 private:
  std::vector<push_cycle<scalar_objpipe, scalar_merger_push>> inputs_;
  fn_type fn_;
  time_point::duration slack_;
  Acceptor acceptor_;
  bool closed_;
};


class scalar_merger_pipe {
 private:
  using scalar_objpipe = expression::scalar_objpipe;

 public:
  using objpipe_errc = objpipe::objpipe_errc;
  using result_type = std::optional<expression::scalar_emit_type>;
  using transport_type = objpipe::detail::transport<result_type>;
  using fn_type = std::function<metric_value(const std::vector<metric_value>&)>;

  scalar_merger_pipe(scalar_merger_pipe&&) = default;

  scalar_merger_pipe(
      std::vector<scalar_objpipe>&& inputs,
      time_point::duration slack,
      fn_type fn);

  auto wait() -> objpipe_errc;
  auto is_pullable() const noexcept -> bool;
  auto front() -> transport_type;
  auto pop_front() -> objpipe_errc;
  auto pull(bool block = true) -> transport_type;
  auto try_pull() -> transport_type;

  template<bool Enable = objpipe::detail::adapt::has_ioc_push<objpipe::detail::adapter_underlying_type_t<scalar_objpipe>, objpipe::existingthread_push>>
  auto can_push(const objpipe::existingthread_push& policy) const
  noexcept
  -> std::enable_if_t<Enable, bool> {
    return std::all_of(
        inputs_.begin(), inputs_.end(),
        [&policy](const auto& x) { return x.can_push(policy); });
  }

  template<typename Acceptor, bool Enable = objpipe::detail::adapt::has_ioc_push<objpipe::detail::adapter_underlying_type_t<scalar_objpipe>, objpipe::existingthread_push>>
  auto ioc_push(const objpipe::existingthread_push& policy, Acceptor&& acceptor) &&
  -> std::enable_if_t<Enable> {
    std::vector<scalar_objpipe> inputs;
    inputs.reserve(inputs_.size());
    std::transform(
        std::make_move_iterator(inputs_.begin()),
        std::make_move_iterator(inputs_.end()),
        std::back_inserter(inputs),
        [](pull_cycle<scalar_objpipe>&& x) {
          return std::move(x).release_source();
        });

    auto push = std::make_shared<scalar_merger_push<std::decay_t<Acceptor>>>(std::forward<Acceptor>(acceptor), std::move(slack_), std::move(fn_));
    push->start(std::move(inputs), policy);
  }

 private:
  std::vector<pull_cycle<scalar_objpipe>> inputs_;
  fn_type fn_;
  time_point::duration slack_;
  bool pending_pop_ = false;
};


template<typename Acceptor>
class vector_merger_push
: public std::enable_shared_from_this<vector_merger_push<Acceptor>>
{
 private:
  using scalar_objpipe = expression::scalar_objpipe;
  using vector_objpipe = expression::vector_objpipe;

 public:
  using fn_type = std::function<metric_value(const std::vector<metric_value>&)>;

  vector_merger_push(
      Acceptor&& acceptor,
      time_point::duration slack,
      fn_type fn,
      std::shared_ptr<const match_clause> mc,
      std::shared_ptr<const match_clause> out_mc)
  : fn_(std::move(fn)),
    slack_(slack),
    acceptor_(std::move(acceptor)),
    mc_(std::move(mc)),
    out_mc_(std::move(out_mc))
  {}

  vector_merger_push(vector_merger_push&&) = delete;

  ~vector_merger_push() noexcept {
    try {
      std::unique_lock<std::mutex> lck{ mtx };
      maybe_emit(lck, true);
    } catch (...) {
      acceptor_.push_exception(std::current_exception());
    }
  }

  auto start(std::vector<std::variant<scalar_objpipe, vector_objpipe>>&& inputs, const objpipe::existingthread_push& policy)
  -> void {
    using this_inputs_variant = std::variant<
        push_cycle<scalar_objpipe, vector_merger_push>,
        push_cycle<vector_objpipe, vector_merger_push>>;
    using scalar_sink_type = typename push_cycle<scalar_objpipe, vector_merger_push>::sink_type;
    using vector_sink_type = typename push_cycle<vector_objpipe, vector_merger_push>::sink_type;

    // First, create all the sinks.
    // Sink type must match the pipe type.
    inputs_.reserve(inputs.size());
    std::transform(
        inputs.cbegin(),
        inputs.cend(),
        std::back_inserter(inputs_),
        [this](const auto& x) -> this_inputs_variant {
          return std::visit(
              overload(
                  [this]([[maybe_unused]] const scalar_objpipe& pipe) -> this_inputs_variant {
                    return push_cycle<scalar_objpipe, vector_merger_push>(scalar_sink_type(), *this);
                  },
                  [this]([[maybe_unused]] const vector_objpipe& pipe) -> this_inputs_variant {
                    return push_cycle<vector_objpipe, vector_merger_push>(vector_sink_type(mc_), *this);
                  }),
              x);
        });

    // Next, start each sink.
    // We do this in a separate step, to easier guarantee that
    // the inputs_ vector won't reallocate.
    // (That may not happen, as it would invalidate pointers.)
    auto task_iter = inputs_.begin();
    for (std::variant<scalar_objpipe, vector_objpipe>& pipe : inputs) {
      assert(task_iter != inputs_.end());
      std::visit(
          overload(
              [this, &policy](push_cycle<scalar_objpipe, vector_merger_push>& task, scalar_objpipe&& pipe) {
                task.start(this->shared_from_this(), std::move(pipe), policy);
              },
              [this, &policy](push_cycle<vector_objpipe, vector_merger_push>& task, vector_objpipe&& pipe) {
                task.start(this->shared_from_this(), std::move(pipe), policy);
              },
              [](const auto& task, const auto& pipe) {
                assert(false); // Sink type and pipe type must match.
              }),
          *task_iter, std::move(pipe));

      ++task_iter;
    }
    assert(task_iter == inputs_.end());
  }

  auto push_exception(std::exception_ptr exptr)
  noexcept
  -> void {
    std::lock_guard<std::mutex> lck{ mtx };
    closed_ = true;
    acceptor_.push_exception(std::move(exptr));
  }

  auto is_closed([[maybe_unused]] const std::unique_lock<std::mutex>& lck) const
  noexcept
  -> bool {
    assert(lck.owns_lock() && lck.mutex() == &mtx);
    return closed_;
  }

  auto maybe_emit([[maybe_unused]] const std::unique_lock<std::mutex>& lck, bool closed = false)
  -> void {
    assert(lck.owns_lock() && lck.mutex() == &mtx);

    while (!this->closed_) {
      // Compute minimum time point.
      // If all of the inputs fail to supply a time point,
      // we're in need of more data.
      time_point tp;
      {
        bool has_suggestion;
        std::tie(tp, has_suggestion) = suggest_emit_tp(inputs_, true);

        if (!has_suggestion)
          return;
      }

      objpipe::objpipe_errc e = acceptor_(compute_vector_emit(tp, inputs_, fn_, *mc_, slack_, out_mc_));
      if (e != objpipe::objpipe_errc::success) closed_ = true;
    }
  }

  std::mutex mtx;

 private:
  std::vector<std::variant<push_cycle<scalar_objpipe, vector_merger_push>, push_cycle<vector_objpipe, vector_merger_push>>> inputs_;
  fn_type fn_;
  time_point::duration slack_;
  Acceptor acceptor_;
  bool closed_;
  const std::shared_ptr<const match_clause> mc_;
  const std::shared_ptr<const match_clause> out_mc_;
};


class vector_merger_pipe {
 private:
  using scalar_objpipe = expression::scalar_objpipe;
  using vector_objpipe = expression::vector_objpipe;
  using input_type = std::variant<
      pull_cycle<scalar_objpipe>,
      pull_cycle<vector_objpipe>>;
  using args_vector = std::vector<std::variant<metric_value, tagged_vector::map_type>>;

 public:
  using objpipe_errc = objpipe::objpipe_errc;
  using result_type = std::vector<expression::vector_emit_type>;
  using transport_type = objpipe::detail::transport<result_type>;
  using fn_type = std::function<metric_value(const std::vector<metric_value>&)>;

  vector_merger_pipe(vector_merger_pipe&&) = default;

  vector_merger_pipe(
      std::vector<std::variant<scalar_objpipe, vector_objpipe>>&& inputs,
      std::shared_ptr<const match_clause> mc,
      std::shared_ptr<const match_clause> out_mc,
      time_point::duration slack,
      fn_type fn);

  auto wait() -> objpipe_errc;
  auto is_pullable() const noexcept -> bool;
  auto front() -> transport_type;
  auto pop_front() -> objpipe_errc;
  auto pull(bool block = true) -> transport_type;
  auto try_pull() -> transport_type;

  template<bool Enable = objpipe::detail::adapt::has_ioc_push<objpipe::detail::adapter_underlying_type_t<scalar_objpipe>, objpipe::existingthread_push>
                                         && objpipe::detail::adapt::has_ioc_push<objpipe::detail::adapter_underlying_type_t<vector_objpipe>, objpipe::existingthread_push>>
  auto can_push(const objpipe::existingthread_push& policy) const
  noexcept
  -> std::enable_if_t<Enable, bool> {
    return std::all_of(
        inputs_.begin(), inputs_.end(),
        [&policy](const auto& x) {
          return std::visit(
              [&policy](const auto& x) { return x.can_push(policy); },
              x);
        });
  }

  template<typename Acceptor, bool Enable = objpipe::detail::adapt::has_ioc_push<objpipe::detail::adapter_underlying_type_t<scalar_objpipe>, objpipe::existingthread_push>
                                         && objpipe::detail::adapt::has_ioc_push<objpipe::detail::adapter_underlying_type_t<vector_objpipe>, objpipe::existingthread_push>>
  auto ioc_push(const objpipe::existingthread_push& policy, Acceptor&& acceptor) &&
  -> std::enable_if_t<Enable> {
    std::vector<std::variant<scalar_objpipe, vector_objpipe>> inputs;
    inputs.reserve(inputs_.size());
    std::transform(
        std::make_move_iterator(inputs_.begin()),
        std::make_move_iterator(inputs_.end()),
        std::back_inserter(inputs),
        [](auto&& x) {
          return std::visit(
              [](auto&& x) -> std::variant<scalar_objpipe, vector_objpipe> {
                return std::move(x).release_source();
              },
              std::move(x));
        });

    auto push = std::make_shared<vector_merger_push<std::decay_t<Acceptor>>>(std::forward<Acceptor>(acceptor), std::move(slack_), std::move(fn_), mc_, out_mc_);
    push->start(std::move(inputs), policy);
  }

 private:
  std::vector<input_type> inputs_;
  fn_type fn_;
  time_point::duration slack_;
  bool pending_pop_ = false;
  std::shared_ptr<const match_clause> mc_;
  std::shared_ptr<const match_clause> out_mc_;
};


scalar_sink::scalar_sink() {}
scalar_sink::~scalar_sink() noexcept {}

auto scalar_sink::suggest_emit_tp() const
noexcept
-> time_point_selector {
  assert(invariant());

  // We make use of the fact that callers are to use
  // forward_to_time to expire emitted facts.
  for (const value& v : data_) {
    if (!v.is_emitted)
      return time_point_selector(v.tp, data_.front().tp);
  }
  if (!data_.empty())
    return time_point_selector(std::optional<time_point>(), data_.front().tp);
  return {};
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

  auto keep_begin = std::lower_bound(
      data_.begin(),
      data_.end(),
      tp,
      tp_compare_());
  if (keep_begin != data_.end() && keep_begin->tp == tp)
    ++keep_begin;

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
            if (!x.is_fact)
              return false;
            else if (recent_.has_value())
              return x.tp > recent_->tp;
            else
              return x.tp >= expire_before;
          }));
  // Validate that we only throw away data at/before tp.
  assert(std::all_of(
          data_.begin(), keep_begin,
          [tp](const value& x) { return x.tp <= tp; }));
  // Validate that only remaining data is after tp.
  assert(std::all_of(
          keep_begin, data_.end(),
          [tp](const value& x) { return x.tp > tp; }));
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
-> time_point_selector {
  assert(invariant());

  return objpipe::of(std::cref(data_))
      .iterate()
      .select_second()
      .transform(&scalar_sink::suggest_emit_tp)
      .reduce(time_point_selector(),
          [](const time_point_selector& x, const time_point_selector& y) {
            return time_point_selector(
                min_of_present_opts(x.speculative, y.speculative),
                min_of_present_opts(x.factual, y.factual));
          });
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

  if (last_known_fact_tp_ == tp)
    last_known_fact_emitted_ = true;

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
#if __cpp_lib_node_extract >= 201703
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
            last_known_fact_emitted_ = false;
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
-> time_point_selector {
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

    if (!next_tp_.factual.has_value() && block) {
      try {
        next = source_.pull();
      } catch (const objpipe::objpipe_error& e) {
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

    if (accepted) {
      if (is_fact && !next_tp_.factual.has_value())
        next_tp_.factual = tp;
      if (!next_tp_.speculative.has_value() || *next_tp_.speculative > tp)
        next_tp_.speculative = tp;
    }

    assert(invariant());
  } while (!accepted_fact);

  assert(invariant());
  return accepted_fact;
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
  next_tp_ = sink_.suggest_emit_tp();
}


scalar_merger_pipe::scalar_merger_pipe(
    std::vector<scalar_objpipe>&& inputs,
    time_point::duration slack,
    fn_type fn)
: fn_(std::move(fn)),
  slack_(slack)
{
  inputs_.reserve(inputs.size());
  std::transform(
      std::make_move_iterator(inputs.begin()),
      std::make_move_iterator(inputs.end()),
      std::back_inserter(inputs_),
      [](scalar_objpipe&& pipe) {
        return pull_cycle<scalar_objpipe>(std::move(pipe), scalar_sink());
      });
}

auto scalar_merger_pipe::wait()
-> objpipe_errc {
  if (pending_pop_) return objpipe_errc::success;

  bool has_suggestion;
  std::tie(std::ignore, has_suggestion) = suggest_emit_tp(inputs_, true);

  return (has_suggestion
      ? objpipe_errc::success
      : objpipe_errc::closed);
}

auto scalar_merger_pipe::is_pullable() const
noexcept
-> bool {
  return std::all_of(
      inputs_.begin(), inputs_.end(),
      [](const auto& x) {
        return x.is_pullable() || x.suggest_emit_tp().get().has_value();
      });
}

auto scalar_merger_pipe::front()
-> transport_type {
  transport_type result = pull();
  assert(result.has_value() || result.errc() != objpipe_errc::success);
  pending_pop_ = true;
  return result;
}

auto scalar_merger_pipe::pop_front()
-> objpipe_errc {
  if (!pending_pop_) return pull().errc();

  pending_pop_ = false;
  return objpipe_errc::success;
}

auto scalar_merger_pipe::pull(bool block)
-> transport_type {
  assert(!pending_pop_);

  // Compute minimum time point.
  // If all of the inputs fails to supply a time point,
  // we're an empty objpipe.
  time_point tp;
  {
    bool has_suggestion;
    std::tie(tp, has_suggestion) = suggest_emit_tp(inputs_, block);

    if (!has_suggestion) {
      if (block
          || std::none_of(
              inputs_.begin(),
              inputs_.end(),
              [](const pull_cycle<scalar_objpipe>& x) {
                return x.is_pullable();
              }))
        return transport_type(std::in_place_index<1>, objpipe_errc::closed);
      else
        return transport_type(std::in_place_index<1>, objpipe_errc::success);
    }
  }

  return transport_type(std::in_place_index<0>, compute_scalar_emit(tp, inputs_, fn_, slack_));
}

auto scalar_merger_pipe::try_pull()
-> transport_type {
  return pull(false);
}


vector_merger_pipe::vector_merger_pipe(
    std::vector<std::variant<scalar_objpipe, vector_objpipe>>&& inputs,
    std::shared_ptr<const match_clause> mc,
    std::shared_ptr<const match_clause> out_mc,
    time_point::duration slack,
    fn_type fn)
: fn_(std::move(fn)),
  slack_(slack),
  mc_(std::move(mc)),
  out_mc_(std::move(out_mc))
{
  inputs_.reserve(inputs.size());
  std::transform(
      std::make_move_iterator(inputs.begin()),
      std::make_move_iterator(inputs.end()),
      std::back_inserter(inputs_),
      [this](std::variant<scalar_objpipe, vector_objpipe>&& pipe) {
        return std::visit(
            overload(
                [](scalar_objpipe&& pipe) -> input_type {
                  return pull_cycle<scalar_objpipe>(std::move(pipe), scalar_sink());
                },
                [this](vector_objpipe&& pipe) -> input_type {
                  return pull_cycle<vector_objpipe>(std::move(pipe), vector_sink(mc_));
                }),
            std::move(pipe));
      });
}

auto vector_merger_pipe::wait()
-> objpipe_errc {
  if (pending_pop_) return objpipe_errc::success;

  if (inputs_.empty()) return objpipe_errc::closed;
  time_point_selector selector;
  std::for_each(
      inputs_.begin(),
      inputs_.end(),
      [&selector](auto& x) {
        std::visit(
            [&selector](auto& x) {
              time_point_selector suggestion = x.suggest_emit_tp();
              if (!suggestion.factual.has_value()) {
                x.read_more(true);
                suggestion = x.suggest_emit_tp();
              }
              selector = tps_min(selector, suggestion);
            },
            x);
      });

  return (selector.get().has_value()
      ? objpipe_errc::success
      : objpipe_errc::closed);
}

auto vector_merger_pipe::is_pullable() const
noexcept
-> bool {
  return std::all_of(
      inputs_.begin(), inputs_.end(),
      [](const auto& x) {
        return std::visit(
            [](const auto& x) {
              return x.is_pullable() || x.suggest_emit_tp().get().has_value();
            },
            x);
      });
}

auto vector_merger_pipe::front()
-> transport_type {
  transport_type result = pull();
  assert(result.has_value() || result.errc() != objpipe_errc::success);
  pending_pop_ = true;
  return result;
}

auto vector_merger_pipe::pop_front()
-> objpipe_errc {
  if (!pending_pop_) return pull().errc();

  pending_pop_ = false;
  return objpipe_errc::success;
}

auto vector_merger_pipe::pull(bool block)
-> transport_type {
  assert(!pending_pop_);

  // Compute minimum time point.
  // If all of the inputs fails to supply a time point,
  // we're an empty objpipe.
  time_point tp;
  {
    bool has_suggestion;
    std::tie(tp, has_suggestion) = suggest_emit_tp(inputs_, block);

    if (!has_suggestion) {
      if (block
          || std::none_of(
              inputs_.begin(),
              inputs_.end(),
              [](const auto& x) {
                return std::visit(
                    [](const auto& x) {
                      return x.is_pullable();
                    },
                    x);
              }))
        return transport_type(std::in_place_index<1>, objpipe_errc::closed);
      else
        return transport_type(std::in_place_index<1>, objpipe_errc::success);
    }
  }

  return transport_type(std::in_place_index<0>, compute_vector_emit(tp, inputs_, fn_, *mc_, slack_, out_mc_));
}

auto vector_merger_pipe::try_pull()
-> transport_type {
  return pull(false);
}


auto wrap_fn_(metric_value(*fn)(const metric_value&, const metric_value&)) ->
std::function<metric_value(const std::vector<metric_value>&)> {
  return [fn](const std::vector<metric_value>& args) {
    assert(args.size() == 2);
    return std::invoke(fn, args[0], args[1]);
  };
}


} /* namespace monsoon::expressions::<unnamed> */


auto make_merger(
    metric_value(*fn)(const metric_value&, const metric_value&),
    time_point::duration slack,
    expression::scalar_objpipe&& x,
    expression::scalar_objpipe&& y)
-> expression::scalar_objpipe {
  std::vector<expression::scalar_objpipe> args;
  args.reserve(2);
  args.push_back(std::move(x));
  args.push_back(std::move(y));

  return objpipe::detail::adapter(scalar_merger_pipe(std::move(args), slack, wrap_fn_(fn)))
      .filter(
          [](const std::optional<expression::scalar_emit_type>& opt) {
            return opt.has_value();
          })
      .deref();
}

auto make_merger(
    metric_value(*fn)(const metric_value&, const metric_value&),
    std::shared_ptr<const match_clause> mc,
    std::shared_ptr<const match_clause> out_mc,
    time_point::duration slack,
    expression::vector_objpipe&& x,
    expression::scalar_objpipe&& y)
-> expression::vector_objpipe {
  std::vector<std::variant<expression::scalar_objpipe, expression::vector_objpipe>> args;
  args.reserve(2);
  args.push_back(std::move(x));
  args.push_back(std::move(y));

  return objpipe::detail::adapter(vector_merger_pipe(std::move(args), mc, out_mc, slack, wrap_fn_(fn)))
      .iterate();
}

auto make_merger(metric_value(*fn)(const metric_value&, const metric_value&),
    std::shared_ptr<const match_clause> mc,
    std::shared_ptr<const match_clause> out_mc,
    time_point::duration slack,
    expression::scalar_objpipe&& x,
    expression::vector_objpipe&& y)
-> expression::vector_objpipe {
  std::vector<std::variant<expression::scalar_objpipe, expression::vector_objpipe>> args;
  args.reserve(2);
  args.push_back(std::move(x));
  args.push_back(std::move(y));

  return objpipe::detail::adapter(vector_merger_pipe(std::move(args), mc, out_mc, slack, wrap_fn_(fn)))
      .iterate();
}

auto make_merger(metric_value(*fn)(const metric_value&, const metric_value&),
    std::shared_ptr<const match_clause> mc,
    std::shared_ptr<const match_clause> out_mc,
    time_point::duration slack,
    expression::vector_objpipe&& x,
    expression::vector_objpipe&& y)
-> expression::vector_objpipe {
  std::vector<std::variant<expression::scalar_objpipe, expression::vector_objpipe>> args;
  args.reserve(2);
  args.push_back(std::move(x));
  args.push_back(std::move(y));

  return objpipe::detail::adapter(vector_merger_pipe(std::move(args), mc, out_mc, slack, wrap_fn_(fn)))
      .iterate();
}

auto make_merger(
    std::function<metric_value(const std::vector<metric_value>&)> fn,
    std::shared_ptr<const match_clause> mc,
    std::shared_ptr<const match_clause> out_mc,
    time_point::duration slack,
    std::vector<std::variant<expression::scalar_objpipe, expression::vector_objpipe>>&& pipes)
-> std::variant<expression::scalar_objpipe, expression::vector_objpipe> {
  if (std::any_of(
          pipes.cbegin(),
          pipes.cend(),
          [](const auto& variant) {
            return std::holds_alternative<expression::scalar_objpipe>(variant);
          })) {
    std::vector<expression::scalar_objpipe> args;
    args.reserve(pipes.size());
    std::transform(
        std::make_move_iterator(pipes.begin()),
        std::make_move_iterator(pipes.end()),
        std::back_inserter(args),
        [](auto&& pipe) -> expression::scalar_objpipe&& {
          return std::get<expression::scalar_objpipe>(std::move(pipe));
        });

    return objpipe::detail::adapter(scalar_merger_pipe(std::move(args), slack, fn))
        .filter(
            [](const std::optional<expression::scalar_emit_type>& opt) {
              return opt.has_value();
            })
        .deref();
  }

  return objpipe::detail::adapter(vector_merger_pipe(std::move(pipes), mc, out_mc, slack, fn))
      .iterate();
}


} /* namespace monsoon::expressions */
