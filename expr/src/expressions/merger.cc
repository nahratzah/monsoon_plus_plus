///\file
///\ingroup expr

#include <monsoon/expressions/merger.h>
#include <deque>
#include <map>
#include <optional>
#include <variant>
#include <monsoon/interpolate.h>
#include <monsoon/metric_value.h>
#include <monsoon/overload.h>
#include <monsoon/tags.h>
#include <monsoon/time_point.h>
#include <monsoon/expression.h>
#include <monsoon/objpipe/of.h>

namespace monsoon {
namespace expressions {


/**
 * \brief Time point value.
 * \ingroup expr
 * \details Describes a scalar or vector at a given time point.
 * \tparam T The type of data held for this time point.
 */
template<typename T>
class tp_value {
 public:
  bool factual;

  ///\brief Default constructor creates a tp_value holding no data.
  constexpr tp_value()
  noexcept
  : factual(false),
    data_(std::in_place_index<1>, nullptr)
  {}

  ///\brief Create a tp_value holding a copy of the given data.
  ///\param[in] data The data to hold.
  ///\param[in] factual Indicates if the data is factual.
  constexpr tp_value(T&& data, bool factual)
  noexcept(std::is_nothrow_move_constructible_v<T>)
  : factual(factual),
    data_(std::in_place_index<0>, std::move(data))
  {}

  ///\brief Create a tp_value holding a copy of the given data.
  ///\param[in] data The data to hold.
  ///\param[in] factual Indicates if the data is factual.
  constexpr tp_value(const T& data, bool factual)
  noexcept(std::is_nothrow_copy_constructible_v<T>)
  : factual(factual),
    data_(std::in_place_index<0>, data)
  {}

  ///\brief Create a tp_value holding a reference to the given data.
  ///\details This constructor omits the copy stage for the data.
  ///\param[in] data A pointer to the data to hold.
  ///\param[in] factual Indicates if the data is factual.
  constexpr tp_value(const T* data, bool factual)
  noexcept
  : factual(factual),
    data_(std::in_place_index<1>, data)
  {
    assert(data != nullptr);
  }

  /**
   * \brief Retrieve the data held in this tp_value.
   * \details If the tp_value holds no data, the result is undefined.
   * \returns A const reference to the held data.
   */
  auto get() const
  noexcept
  -> const T& {
    return std::visit(
        overload(
            [](const T& v) { return v; },
            [](const T* v) { assert(v != nullptr); return *v; }),
        data_);
  }

 private:
  std::variant<T, const T*> data_;
};

class scalar_sink {
 private:
  struct speculative_data {
    speculative_data() = default;

    speculative_data(time_point tp, expression::speculative_scalar data)
    : tp(std::move(tp)),
      data(std::move(data))
    {}

    time_point tp;
    expression::speculative_scalar data;
  };

  struct factual_data {
    factual_data() = default;

    factual_data(time_point tp, expression::factual_scalar data)
    : tp(std::move(tp)),
      data(std::move(data))
    {}

    time_point tp;
    expression::factual_scalar data;
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
   * \param tp The time point for which a value is requested.
   * \returns Value at the given time point.
   */
  auto get(time_point tp) const -> tp_value<metric_value>;

  /**
   * \brief Ensure the buffered, emittable time points are all after the given time point.
   *
   * \details Ensures the only pending data is after the given time point.
   * \param[in] tp The time point to forward to.
   */
  auto forward_to_time(time_point tp) -> void;

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
   * \brief Update speculative values, erasing all at or before the given time point.
   *
   * \details Ensures the speculative set contains only values after \p tp.
   * \param tp The time point up to which to erase speculative values.
   */
  auto update_speculative_for_tp_(time_point tp) -> void;

  /**
   * \brief List of speculative values.
   *
   * \details
   * List is ordered by time_point.
   * Each element has a time point after the most recent factual.
   */
  std::deque<speculative_data> speculative_;

  /**
   * \brief List of factual values.
   *
   * \details
   * List is ordered by time_point.
   * Each element has a time point after recent_->tp.
   *
   * Time points are unique.
   */
  std::deque<factual_data> factual_;

  /**
   * \brief Last factual before the forwarding time point.
   *
   * \details
   * This factual has a time point before factual_.front().tp.
   *
   * It is used in interpolations between it and factual_.front().
   */
  std::optional<factual_data> recent_;
};

class vector_sink {
 public:
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
   * \param tp The time point for which a value is requested.
   * \returns Value at the given time point.
   */
  auto get(time_point tp) const -> tp_value<std::unordered_map<tags, metric_value>>;

  /**
   * \brief Ensure the buffered, emittable time points are all after the given time point.
   *
   * \details Ensures the only pending data is after the given time point.
   * \param[in] tp The time point to forward to.
   */
  auto forward_to_time(time_point tp) -> void;

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


scalar_sink::scalar_sink() {}

scalar_sink::~scalar_sink() noexcept {}

auto scalar_sink::suggest_emit_tp() const
noexcept
-> std::optional<time_point> {
  if (factual_.empty()) return {};
  return factual_.front().tp;
}

auto scalar_sink::forward_to_time(time_point tp)
-> void {
  assert(invariant());

  auto fact_end = std::upper_bound(
      factual_.begin(), factual_.end(),
      tp,
      overload(
          [](const speculative_data& x, time_point y) { return x.tp < y; },
          [](time_point x, const speculative_data& y) { return x < y.tp; },
          [](const factual_data& x, time_point y) { return x.tp < y; },
          [](time_point x, const factual_data& y) { return x < y.tp; }));
  if (fact_end != factual_.begin())
    recent_.emplace(std::move(*std::prev(fact_end)));
  factual_.erase(factual_.begin(), fact_end);

  assert(invariant());
}

auto scalar_sink::accept(expression::scalar_emit_type&& emt)
-> bool {
  assert(invariant());

  time_point& tp = emt.tp;
  bool accepted = false;
  switch (emt.data.index()) {
    default:
      assert(false);
      break;
    case 0: // Speculative
      {
        auto& s = std::get<0>(emt.data);

        if (!factual_.empty()) {
          if (tp <= factual_.back().tp) break; // Skip
        } else if (recent_.has_value() && tp <= recent_->tp) {
          break; // Skip
        }

        if (speculative_.empty() || speculative_.back().tp < tp) {
          speculative_.emplace_back(tp, std::move(s));
        } else if (speculative_.empty() || speculative_.back().tp == tp) {
          speculative_.back().data = std::move(s);
        } else {
          auto ins_pos = std::lower_bound(speculative_.begin(), speculative_.end(),
              tp,
              overload(
                  [](const speculative_data& x, time_point y) { return x.tp < y; },
                  [](time_point x, const speculative_data& y) { return x < y.tp; }));
          if (ins_pos != speculative_.end() && ins_pos->tp == tp)
            ins_pos->data = std::move(s);
          else
            speculative_.emplace(std::move(ins_pos), std::move(tp), std::move(s));
        }

        accepted = true;
      }
      break;
    case 1: // Factual
      {
        auto& s = std::get<1>(emt.data);

        if (recent_.has_value())
          assert(tp > recent_->tp); // Factual must be strictly ordered.
        if (!factual_.empty())
          assert(tp > factual_.back().tp); // Factual must be strictly ordered.

        update_speculative_for_tp_(tp);
        factual_.emplace_back(std::move(tp), std::move(s));

        accepted = true;
      }
      break;
  }

  assert(invarant());
  return accepted;
}

auto scalar_sink::update_speculative_for_tp_(time_point tp)
-> void {
  assert(invariant());

  auto spec_end = std::upper_bound(
      speculative_.begin(), speculative_.end(),
      tp,
      overload(
          [](const speculative_data& x, time_point y) { return x.tp < y; },
          [](time_point x, const speculative_data& y) { return x < y.tp; }));
  speculative_.erase(speculative_.begin(), spec_end);

  assert(invariant());
}

auto scalar_sink::invariant() const
noexcept
-> bool {
  if (recent_.has_value() && !factual_.empty()) {
    if (recent_->tp >= factual_.front().tp)
      return false;
  }

  if (!factual_.empty() && !speculative_.empty()) {
    if (factual_.back().tp >= speculative_.front().tp)
      return false;
  }

  if (!std::is_sorted(factual_.begin(), factual_.end(),
          [](const auto& x, const auto& y) { return x.tp < y.tp; }))
    return false;

  if (!std::is_sorted(speculative_.begin(), speculative_.end(),
          [](const auto& x, const auto& y) { return x.tp < y.tp; }))
    return false;

  return true;
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

auto vector_sink::forward_to_time(time_point tp)
-> void {
  assert(invariant());

  for (auto& elem : data_) elem.second.forward_to_time(tp);

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
      .transform(&scalar_sink::invariant)
      .reduce(std::logical_and<bool>())
      .value_or(true);
}


}} /* namespace monsoon::expressions */
