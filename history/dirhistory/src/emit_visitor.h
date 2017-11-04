#ifndef EMIT_VISITOR_H
#define EMIT_VISITOR_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <monsoon/history/dir/tsdata.h>
#include <type_traits>
#include <queue>
#include <memory>
#include <optional>
#include <cassert>
#include <monsoon/time_point.h>
#include <monsoon/time_range.h>
#include <boost/coroutine2/coroutine.hpp>

namespace monsoon {
namespace history {


class monsoon_dirhistory_local_ basic_emit_visitor {
 private:
  struct tsdata_heap_cmp {
    bool operator()(const std::shared_ptr<tsdata>& x,
        const std::shared_ptr<tsdata>& y) const {
      assert(x != nullptr && y != nullptr);
      return std::get<0>(x->time()) > std::get<0>(y->time());
    }
  };

  using selected_files_heap = std::priority_queue<
      std::shared_ptr<tsdata>,
      std::vector<std::shared_ptr<tsdata>>,
      tsdata_heap_cmp>;

 protected:
  basic_emit_visitor(const std::vector<std::shared_ptr<tsdata>>&,
      const time_range&, time_point::duration);
  ~basic_emit_visitor() noexcept;

 private:
  static auto select_files(const std::vector<std::shared_ptr<tsdata>>&,
      const std::optional<time_point>&, const std::optional<time_point>&)
      -> selected_files_heap;

 protected:
  std::optional<time_point> sel_begin_, sel_end_;
  const time_point::duration slack_;
  const time_range tr_;
  selected_files_heap files_;
};

template<typename... Args>
class monsoon_dirhistory_local_ emit_visitor
: public basic_emit_visitor
{
 private:
  using co_arg_type = std::tuple<
      time_point,
      std::conditional_t<
          std::is_const_v<Args>,
          std::add_lvalue_reference_t<Args>,
          std::add_rvalue_reference_t<Args>>...>;
  using co_type = boost::coroutines2::coroutine<co_arg_type>;

  using iteration = std::tuple<time_point, std::decay_t<Args>...>;
  using iteration_heap =
      std::vector<std::pair<iteration, typename co_type::pull_type>>;

  struct iteration_heap_cmp {
    bool operator()(
        typename iteration_heap::const_reference x,
        typename iteration_heap::const_reference& y) const {
      return std::get<0>(x.first) > std::get<0>(y.first);
    }
  };

 public:
  using callback = std::function<void(time_point, std::add_rvalue_reference_t<Args>...)>;
  using invocation_functor =
      std::function<void(const tsdata&, const callback&, const std::optional<time_point>&, const std::optional<time_point>&)>;
  using reduce_queue = std::deque<iteration>;
  using pruning_vector = std::vector<iteration>;

  using merge_dst_tuple = std::add_lvalue_reference_t<iteration>;
  using merge_src_tuple = std::add_rvalue_reference_t<co_arg_type>;
  using merge_functor = std::function<void(merge_dst_tuple, merge_src_tuple)>;
  using reduce_at_functor =
      std::function<iteration(time_point, const reduce_queue&)>;
  using pruning_functor = std::function<void(pruning_vector&)>;

  emit_visitor(const std::vector<std::shared_ptr<tsdata>>&,
      const time_range&, time_point::duration,
      invocation_functor,
      merge_functor,
      reduce_at_functor,
      pruning_functor prune_before,
      pruning_functor prune_after);

  void operator()(const callback&);
  void before(const callback&);
  void during(const callback&);
  void after(const callback&);

 private:
  auto file_to_coroutine_(const std::shared_ptr<tsdata>&) const
      -> typename co_type::pull_type;
  auto iteration_tp_() -> std::optional<time_point>;
  auto iteration_value_() -> iteration;
  template<typename Fn> void iteration_value_(Fn&&);
  void ival_pending_cleanup_();

  void before_with_interval_(const callback&);
  void during_with_interval_(const callback&);
  void after_with_interval_(const callback&);

  void before_without_interval_(const callback&,
      const std::optional<time_point>&);
  void during_without_interval_(const callback&,
      const std::optional<time_point>&);
  void after_without_interval_(const callback&);
  void emit_without_interval_(const callback&);

  iteration_heap iteration_;
  invocation_functor invoc_;
  merge_functor merge_;
  reduce_at_functor reduce_at_;
  pruning_functor prune_before_, prune_after_;
  time_point ival_iter_;
  std::deque<iteration> ival_pending_;
};


}} /* namespace monsoon::history */

#include "emit_visitor-inl.h"

#endif /* EMIT_VISITOR_H */
