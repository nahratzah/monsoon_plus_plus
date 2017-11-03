#ifndef MONSOON_EXPRESSIONS_MERGER_H
#define MONSOON_EXPRESSIONS_MERGER_H

///\file
///\ingroup expr

#include <monsoon/objpipe/pull_reader.h>
#include <monsoon/expr_export_.h>
#include <monsoon/expression.h>
#include <monsoon/time_point.h>
#include <monsoon/tags.h>
#include <monsoon/metric_value.h>
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <type_traits>
#include <utility>
#include <mutex>
#include <condition_variable>

namespace monsoon {
namespace expressions {


class monsoon_expr_export_ scalar_accumulator {
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


class monsoon_expr_export_ vector_accumulator {
 private:
  struct monsoon_expr_local_ speculative_cmp {
    using is_transparent = std::true_type;

    bool operator()(
        std::tuple<time_point, const tags&> x,
        std::tuple<time_point, const tags&> y) const noexcept;
    bool operator()(
        std::tuple<time_point, const tags&> x,
        const time_point& y) const noexcept;
    bool operator()(
        const time_point& x,
        std::tuple<time_point, const tags&> y) const noexcept;
  };

  using speculative_map = std::map<
      std::tuple<time_point, tags>,
      metric_value,
      speculative_cmp>;

  struct monsoon_expr_local_ speculative_index_cmp {
   private:
    using internal_type = std::tuple<const tags&, time_point>;

   public:
    using is_transparent = std::true_type;

    template<typename X, typename Y>
    bool operator()(const X& x, const Y& y) const noexcept;

   private:
    static auto intern_(const speculative_map::const_iterator i) noexcept
        -> internal_type;
    static auto intern_(const internal_type& i) noexcept
        -> const internal_type&;
    static auto intern_(std::tuple<time_point, const tags&> i) noexcept
        -> internal_type;
  };

  using speculative_index = std::set<
      speculative_map::const_iterator,
      speculative_index_cmp>;
  using factual_list =
      std::deque<std::pair<time_point, expression::factual_vector>>;

 public:
  struct tp_proxy {
   public:
    tp_proxy(const vector_accumulator& self, time_point tp) noexcept;
    auto operator[](const tags& tag_set) const
        -> std::optional<std::tuple<metric_value, bool>>;
    bool is_speculative() const noexcept;
    auto value() const
        -> std::variant<
            expression::factual_vector,
            std::reference_wrapper<const expression::factual_vector>>;

   private:
    const vector_accumulator& self_;
    time_point tp_;
  };

  auto operator[](time_point tp) const -> tp_proxy;
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
          std::reference_wrapper<const expression::factual_vector>>;

  speculative_map speculative_;
  factual_list factual_;
  speculative_index speculative_index_;
};


namespace {


/**
 * \brief Managed accumulator with associated input objpipe.
 *
 * \tparam ObjPipe The input object pipe.
 */
template<typename ObjPipe>
class merger_managed {
 public:
  static_assert(std::is_same_v<expression::scalar_objpipe, ObjPipe>
      || std::is_same_v<expression::vector_objpipe, ObjPipe>,
      "merger_managed needs either a scalar or vector objpipe template argument");

  ///\brief The accumulator type managed by this.
  using accumulator_type = std::conditional_t<
      std::is_same_v<expression::scalar_objpipe, ObjPipe>,
      scalar_accumulator,
      vector_accumulator>;

  /**
   * \brief Constructor.
   * \param input The input objpipe.
   */
  explicit merger_managed(ObjPipe&& input);

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
  bool load_until_next_factual(Callback callback);

  /**
   * \brief Get the time point of the most recently seen factual emition.
   * \return The \ref time_point of the most recently seen factual,
   *   or an empty optional if no factuals have been seen.
   */
  auto factual_until() const noexcept -> std::optional<time_point>;

  /**
   * \brief Test if the input is pullable.
   * \return True if the input has pending elements or an attached writer.
   */
  bool is_pullable() const noexcept;

  /**
   * \brief Discard all time points up to \p tp.
   * \param tp The minimum time point which has to remain valid.
   */
  void advance_factual(time_point tp);

  ///\brief The managed accumulator state.
  accumulator_type accumulator;

 private:
  ObjPipe input_;
};


/**
 * \brief Unpack operation.
 */
struct unpack_ {
  using tag_set_t = const tags*;

  explicit unpack_(const expression::vector_emit_type& v);
  explicit unpack_(const expression::scalar_emit_type& v);

  auto operator()(const scalar_accumulator& m) -> std::optional<metric_value>;
  auto operator()(const vector_accumulator& m)
      -> std::optional<std::variant<
          std::tuple<tags, metric_value>,
          expression::factual_vector,
          std::reference_wrapper<const expression::factual_vector>>>;

  const time_point tp;
  const tag_set_t tag_set = nullptr;
  bool speculative;
};

struct left_tag_combiner_ {
  auto operator()(const tags& x, const tags&) const noexcept -> const tags&;
  auto operator()(tags&& x, const tags&) const noexcept -> tags&&;
};


/**
 * \brief Apply untagged metric values, tagged metric values,
 * and vectors of metric values to a functor.
 *
 * \details
 * The functor must have a signature of either:
 * \code
 *   void(const std::array<metric_value, N>&)
 * \endcode
 * for untagged values, or:
 * \code
 *   void(tags, const std::array<metric_value, N>&)
 * \endcode
 * for tagged metric values.
 *
 * \tparam Fn The functor to be invoked.
 * \tparam TagEqual A functor to test for tag equality.
 * \tparam TagCombine A tag combiner.
 */
template<typename Fn,
    typename TagEqual = std::equal_to<tags>,
    typename TagCombine = left_tag_combiner_>
class recursive_apply {
 private:
  ///\brief Discriminant for the case where no tags are present.
  struct untagged {};

 public:
  explicit recursive_apply(Fn fn,
      TagEqual equal = TagEqual(),
      TagCombine combine = TagCombine());

  template<typename... Values>
  void operator()(Values&&... values);

 private:
  ///\brief Untagged sentinel invocation.
  template<std::size_t N>
  void recursive_apply_(
      const untagged&,
      const std::array<metric_value, N>& values);
  ///\brief Tagged sentinel invocation.
  template<std::size_t N>
  void recursive_apply_(
      const tags& tags,
      const std::array<metric_value, N>& values);
  ///\brief Unpack optional.
  template<typename Tags, std::size_t N, typename Head, typename... Tail>
  void recursive_apply_(
      const Tags& tags,
      std::array<metric_value, N>& values,
      const std::optional<Head>& head,
      Tail&&... tail);
  ///\brief Unpack reference wrapper.
  template<typename Tags, std::size_t N, typename Head, typename... Tail>
  void recursive_apply_(
      const Tags& tags,
      std::array<metric_value, N>& values,
      const std::reference_wrapper<Head>& head,
      Tail&&... tail);
  ///\brief Unpack variant.
  template<typename Tags, std::size_t N, typename... Head, typename... Tail>
  void recursive_apply_(
      const Tags& tags,
      std::array<metric_value, N>& values,
      const std::variant<Head...>& head,
      Tail&&... tail);
  ///\brief Unpack tagged metric.
  template<std::size_t N, typename... Tail>
  void recursive_apply_(
      const untagged&,
      std::array<metric_value, N>& values,
      const std::tuple<tags, metric_value>& head,
      Tail&&... tail);
  ///\brief Unpack tagged metric.
  template<std::size_t N, typename... Tail>
  void recursive_apply_(
      const tags& tag_set,
      std::array<metric_value, N>& values,
      const std::tuple<tags, metric_value>& head,
      Tail&&... tail);
  ///\brief Unpack untagged metric.
  template<typename Tags, std::size_t N, typename... Tail>
  void recursive_apply_(
      const Tags& tags,
      std::array<metric_value, N>& values,
      const metric_value& head,
      Tail&&... tail);
  ///\brief Unpack tagged metric set, when there is no known tag.
  template<std::size_t N, typename... MapArgs, typename... Tail>
  void recursive_apply_(
      const untagged&,
      std::array<metric_value, N>& values,
      const std::unordered_map<tags, metric_value, MapArgs...>& head,
      Tail&&... tail);
  ///\brief Unpack tagged metric set, when there is a known tag.
  template<std::size_t N, typename... MapArgs, typename... Tail>
  void recursive_apply_(
      const tags& tag_set,
      std::array<metric_value, N>& values,
      const std::unordered_map<tags, metric_value, MapArgs...>& head,
      Tail&&... tail);

  Fn fn_;
  TagEqual tag_equal_;
  TagCombine tag_combine_;
};

template<typename Fn, typename TagEqual, typename TagCombine>
auto make_recursive_apply(Fn&& fn, TagEqual&& equal, TagCombine&& tag_combine)
    -> recursive_apply<
        std::decay_t<Fn>,
        std::decay_t<TagEqual>,
        std::decay_t<TagCombine>>;

template<typename Fn>
auto make_recursive_apply(Fn&& fn) -> recursive_apply<std::decay_t<Fn>>;


/**
 * \brief Helper for merger that handles accepting a value for \p CbIdx
 * and combines it with other managed accumulators.
 *
 * \tparam CbIdx The index of the managed accumulator for which the value(s)
 * are received.
 */
template<std::size_t CbIdx>
struct merger_invocation {
  ///\brief Invoke functor on managed accumulators upon receival of value.
  template<typename Fn, typename... Managed, typename Value>
  void operator()(Fn& fn, const std::tuple<Managed...>& managed,
      const Value& v);

 private:
  ///\brief Helper for operator(), uses indices to handle value extraction.
  template<typename Fn, typename... Managed, typename Value,
      std::size_t... Indices>
  void invoke_(Fn& fn, const std::tuple<Managed...>& managed,
      const Value& v,
      std::index_sequence<Indices...> indices);

  ///\brief Apply unpacked values.
  template<typename Fn, typename... Values, std::size_t... Indices>
  void unpack_invoke_(
      const unpack_& unpack,
      std::index_sequence<Indices...>,
      Fn& fn,
      time_point tp,
      Values&&... values);

  template<typename Value, bool ApplyUnpack>
  struct unpack_result_type {
    using type = decltype(std::declval<unpack_&>()(std::declval<Value>()));
  };
  template<typename Value>
  struct unpack_result_type<Value, true> {
    using type = Value;
  };

  ///\brief Apply unpack on managed object, but pass value through unmodified.
  template<std::size_t Idx, typename Value>
  auto apply_unpack_(unpack_& unpack, Value& value)
      -> typename unpack_result_type<Value&, Idx == CbIdx>::type;

  ///\brief Return the managed accumulator, expect for the indexed value.
  template<std::size_t Idx, typename Managed, typename Value>
  constexpr auto choose_value_(const Managed& managed, const Value& value)
      -> std::conditional_t<
          Idx == CbIdx,
          const Value&,
          const typename Managed::accumulator_type&>;
};


/**
 * \brief Base class for merger_acceptor.
 *
 * \details
 * This class holds types and some values, to reduce duplication
 * and allow extracting types with fewer arguments.
 * It is not designed to be used standalone.
 *
 * \tparam IsTagged True if the merger_acceptor is to hold tagged data.
 */
template<bool IsTagged>
class merger_acceptor_data {
 private:
  using speculative_type = std::conditional_t<
      !IsTagged,
      expression::speculative_scalar,
      expression::speculative_vector>;
  using factual_type = std::conditional_t<
      !IsTagged,
      expression::factual_scalar,
      expression::factual_vector>;

 protected:
  merger_acceptor_data() = default;
  merger_acceptor_data(const merger_acceptor_data&) = default;
  merger_acceptor_data(merger_acceptor_data&&) = default;
  merger_acceptor_data& operator=(const merger_acceptor_data&) = default;
  merger_acceptor_data& operator=(merger_acceptor_data&&) = default;
  ~merger_acceptor_data() = default;

 public:
  using speculative_entry = std::pair<time_point, speculative_type>;
  using factual_entry = std::pair<time_point, factual_type>;

  std::optional<factual_entry> factual;
};


template<typename, typename, bool, std::size_t> class merger_acceptor;

template<typename Fn, typename SpecInsIter, std::size_t N>
class merger_acceptor<Fn, SpecInsIter, false, N>
: public merger_acceptor_data<false>
{
 public:
  merger_acceptor(Fn& fn, SpecInsIter speculative);

  void operator()(
      bool is_factual,
      time_point tp,
      const std::array<metric_value, N>& values);

 private:
  Fn& fn_;
  SpecInsIter speculative;
};

template<typename Fn, typename SpecInsIter, std::size_t N>
class merger_acceptor<Fn, SpecInsIter, true, N>
: public merger_acceptor_data<true>
{
 public:
  merger_acceptor(Fn& fn, SpecInsIter speculative);

  void operator()(
      bool is_factual,
      time_point tp,
      const tags& tag_set,
      const std::array<metric_value, N>& values);

 private:
  Fn& fn_;
  SpecInsIter speculative;
};


template<typename... ObjPipes>
constexpr bool tagged_v =
    std::disjunction_v<std::is_same<expression::vector_objpipe, ObjPipes>...>;


} /* namespace monsoon::expressions::<unnamed> */


template<typename Fn, typename... ObjPipes>
class merger final
: public monsoon::objpipe::pull_reader<
      typename std::conditional_t<
         tagged_v<ObjPipes...>,
         expression::vector_objpipe,
         expression::scalar_objpipe
      >::value_type>
{
  static_assert(
      std::conjunction_v<
          std::disjunction<
              std::is_same<expression::vector_objpipe, ObjPipes>,
              std::is_same<expression::scalar_objpipe, ObjPipes>>...>,
      "Merger requires that all object pipes are either scalar or vector object pipes.");

 public:
  using objpipe_errc = monsoon::objpipe::objpipe_errc;
  using value_type =
      typename std::conditional_t<
         tagged_v<ObjPipes...>,
         expression::vector_objpipe,
         expression::scalar_objpipe
      >::value_type;
  static constexpr bool tagged = tagged_v<ObjPipes...>;

 private:
  class read_invocation_ {
    using invocation_fn = auto (merger::*)()
        -> std::tuple<std::optional<time_point>, bool>;

   public:
    read_invocation_() = default;
    explicit read_invocation_(invocation_fn fn) noexcept;

    bool is_pullable() const noexcept;
    void operator()(merger& self);

    // Equality comparison makes no sense and is thus omitted.
    bool operator<(const read_invocation_& other) const noexcept;
    bool operator>(const read_invocation_& other) const noexcept;

   private:
    std::optional<time_point> factual_until_;
    invocation_fn invocation_ = nullptr;
  };

  using read_invoc_array = std::array<read_invocation_, sizeof...(ObjPipes)>;
  using speculative_entry =
      typename merger_acceptor_data<tagged>::speculative_entry;
  using factual_entry =
      typename merger_acceptor_data<tagged>::factual_entry;

 public:
  explicit merger(const Fn& fn, ObjPipes&&... inputs);
  explicit merger(Fn&& fn, ObjPipes&&... inputs);

 private:
  auto try_next() -> std::variant<value_type, objpipe::no_value_reason>
      override;
  auto is_pullable_impl() const -> bool override;
  auto empty_impl() const -> bool override;

  ///\brief Try to read elements until we encounter a factual entry.
  void try_fill_();

  ///\brief Wait for data to become available.
  auto wait_for_data() const -> std::optional<objpipe::no_value_reason>
      override;

  ///\brief Invocation for a specific managed accumulator, to retrieve values.
  template<std::size_t CbIdx>
  auto load_until_next_factual_()
      -> std::tuple<std::optional<time_point>, bool>;

  ///\brief Invoke \p fn on each element of tuple managed_.
  template<typename ManFn>
  void for_each_managed_(ManFn fn);

  ///\brief Sentinel recursive invocation of \ref for_each_managed_().
  template<typename ManFn>
  void for_each_managed_impl_(ManFn& fn, std::index_sequence<>) noexcept;

  ///\brief Recursing invoker implementation of \ref for_each_managed_().
  template<typename ManFn, std::size_t Idx, std::size_t... Tail>
  void for_each_managed_impl_(ManFn& fn, std::index_sequence<Idx, Tail...>);

  ///\brief Initializer for read_invocations_ member variable.
  template<std::size_t... Indices>
  static constexpr auto new_read_invocations_(std::index_sequence<Indices...>)
      noexcept
      -> std::array<read_invocation_, sizeof...(Indices)>;

  std::tuple<merger_managed<ObjPipes>...> managed_;
  const Fn fn_;
  read_invoc_array read_invocations_ =
      new_read_invocations_(std::index_sequence_for<ObjPipes...>());
  std::deque<speculative_entry> speculative_pending_;
  std::deque<factual_entry> factual_pending_;
  mutable std::mutex mtx_;
  mutable std::condition_variable read_avail_;
};

template<typename Fn, typename... ObjPipe>
auto make_merger(Fn&& fn, ObjPipe&&... inputs)
    -> objpipe::reader<typename merger<
        std::decay_t<Fn>,
        std::decay_t<ObjPipe>...>::value_type>;


}} /* namespace monsoon::expressions */

#include "merger-inl.h"

#endif /* MONSOON_EXPRESSIONS_MERGER_H */
