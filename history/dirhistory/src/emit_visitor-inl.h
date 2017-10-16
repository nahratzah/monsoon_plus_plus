#ifndef EMIT_VISITOR_INL_H
#define EMIT_VISITOR_INL_H

#include <boost/coroutine2/protected_fixedsize_stack.hpp>
#include <list>

namespace monsoon {
namespace history {


template<typename... Args>
emit_visitor<Args...>::emit_visitor(
    const std::vector<std::shared_ptr<tsdata>>& files,
    const time_range& tr, time_point::duration slack,
    invocation_functor invoc,
    merge_functor merge,
    reduce_at_functor reduce_at,
    pruning_functor prune_before,
    pruning_functor prune_after)
: basic_emit_visitor(files, tr, slack),
  invoc_(std::move(invoc)),
  merge_(std::move(merge)),
  reduce_at_(std::move(reduce_at)),
  prune_before_(std::move(prune_before)),
  prune_after_(std::move(prune_after))
{
  assert(invoc_ != nullptr);
  assert(merge_ != nullptr);
  assert(reduce_at_ != nullptr);
  assert(prune_before_ != nullptr);
  assert(prune_after_ != nullptr);
}

template<typename... Args>
void emit_visitor<Args...>::operator()(const callback& cb) {
  before(cb);
  during(cb);
  after(cb);
}

template<typename... Args>
void emit_visitor<Args...>::before(const callback& cb) {
  if (!tr_.begin().has_value()) return;

  if (tr_.interval().has_value())
    before_with_interval_(cb);
  else
    before_without_interval_(cb, tr_.begin());
}

template<typename... Args>
void emit_visitor<Args...>::during(const callback& cb) {
  if (tr_.interval().has_value())
    during_with_interval_(cb);
  else
    during_without_interval_(cb, tr_.end());
}

template<typename... Args>
void emit_visitor<Args...>::after(const callback& cb) {
  if (tr_.interval().has_value())
    after_with_interval_(cb);
  else
    after_without_interval_(cb);
}

template<typename... Args>
auto emit_visitor<Args...>::file_to_coroutine_(
    const std::shared_ptr<tsdata>& ptr) const
-> typename co_type::pull_type
{
  using push_type = typename co_type::push_type;
  using pull_type = typename co_type::pull_type;

  return pull_type(
      boost::coroutines2::protected_fixedsize_stack(),
      [ptr, this](push_type& yield) {
        std::invoke(
            invoc_, *ptr,
            [&yield](time_point tp, auto&&... args) {
              yield(
                  std::forward_as_tuple(
                      tp,
                      std::forward<std::decay_t<decltype(args)>>(args)...));
            },
            sel_begin_, sel_end_);
      });
}

template<typename... Args>
auto emit_visitor<Args...>::iteration_tp_() -> std::optional<time_point> {
  while (!files_.empty() &&
      (iteration_.empty()
       || std::get<0>(iteration_.front().first) > std::get<0>(files_.top()->time()))) {
    auto co = file_to_coroutine_(files_.top());
    files_.pop();
    if (co) {
      auto co_val = co.get();
      iteration_.emplace_back(std::move(co_val), std::move(co));
      std::push_heap(iteration_.begin(), iteration_.end(), iteration_heap_cmp());
    }
  }

  if (iteration_.empty()) return {};
  return std::get<0>(iteration_.front().first);
}

template<typename... Args>
auto emit_visitor<Args...>::iteration_value_() -> iteration {
  assert(!iteration_.empty());

  std::pop_heap(iteration_.begin(), iteration_.end(), iteration_heap_cmp());
  auto& paired_value = iteration_.back();
  iteration result = std::move(paired_value.first);  // Extract pending result.
  paired_value.second();  // Advance coroutine.

  if (!paired_value.second) {  // Coroutine depleted.
    iteration_.pop_back();
  } else {
    paired_value.first = paired_value.second.get();
    std::push_heap(iteration_.begin(), iteration_.end(), iteration_heap_cmp());
  }

  return result;
}

template<typename... Args>
template<typename Fn>
void emit_visitor<Args...>::iteration_value_(Fn&& cb) {
  assert(!iteration_.empty());

  std::pop_heap(iteration_.begin(), iteration_.end(), iteration_heap_cmp());
  auto& paired_value = iteration_.back();
  std::invoke(cb, std::move(paired_value.first));  // Extract pending result.
  paired_value.second();  // Advance coroutine.

  if (!paired_value.second()) {  // Coroutine depleted.
    iteration_.pop_back();
  } else {
    paired_value.first = paired_value.second.get();
    std::push_heap(iteration_.begin(), iteration_.end(), iteration_heap_cmp());
  }
}

template<typename... Args>
void emit_visitor<Args...>::ival_pending_cleanup_() {
  // Remove all expired entries.
  while (std::get<0>(ival_pending_.front()) < ival_iter_ - slack_)
    ival_pending_.pop_front();
}

template<typename... Args>
void emit_visitor<Args...>::before_with_interval_(const callback& cb) {
  // Initialize begin.
  if (tr_.begin().has_value())
    ival_iter_ = tr_.begin().value();
  else {
    auto tp0 = iteration_tp_();
    if (tp0.has_value() &&
        (!tr_.end().has_value() || tr_.end().value() > tp0.value())) {
      ival_iter_ = tp0.value();
    } else if (tr_.end().has_value()) {
      ival_iter_ = tr_.end().value();
      return;
    }
  }

  before_without_interval_(
      [this](auto&&... args) {
        ival_pending_.emplace_back(std::forward<std::decay_t<decltype(args)>>(args)...);
      },
      ival_iter_ + slack_);

  std::apply(cb, std::invoke(reduce_at_, ival_iter_, ival_pending_));
  ival_iter_ += tr_.interval().value();
}

template<typename... Args>
void emit_visitor<Args...>::during_with_interval_(const callback& cb) {
  during_without_interval_(
      [this, &cb](time_point tp0, auto&&... args) {
        while (tp0 > ival_iter_ + slack_
            && (!tr_.end().has_value() || tp0 < tr_.end())) {
          ival_pending_cleanup_();
          std::apply(cb, std::invoke(reduce_at_, ival_iter_, ival_pending_));

          ival_iter_ += tr_.interval().value();
        }

        ival_pending_.emplace_back(tp0, std::forward<std::decay_t<decltype(args)>>(args)...);
      },
      sel_end_);

  // Deal with read, pending elements
  // (relevant if end() extends past the history range).
  if (tr_.end().has_value()) {
    while (ival_iter_ < tr_.end()) {
      ival_pending_cleanup_();
      std::apply(cb, std::invoke(reduce_at_, ival_iter_, ival_pending_));

      ival_iter_ += tr_.interval().value();
    }
  }
}

template<typename... Args>
void emit_visitor<Args...>::after_with_interval_(const callback& cb) {
  if (!tr_.end().has_value()) return;
  assert(ival_iter_ >= tr_.end().value());

  ival_iter_ = tr_.end().value();
  ival_pending_cleanup_();

  after_without_interval_(
      [this](auto&&... args) {
        ival_pending_.emplace_back(std::forward<std::decay_t<decltype(args)>>(args)...);
      });

  std::apply(cb, std::invoke(reduce_at_, ival_iter_, ival_pending_));
}

template<typename... Args>
void emit_visitor<Args...>::before_without_interval_(const callback& cb,
    const std::optional<time_point>& tr_begin) {
  if (!tr_begin.has_value()) return;

  pruning_vector iterations;
  for (auto itp = iteration_tp_();
      itp.has_value() && itp <= tr_begin.value();
      itp = iteration_tp_()) {
    emit_without_interval_(
        [&iterations](auto&&... args) {
          iterations.emplace_back(std::forward<std::decay_t<decltype(args)>>(args)...);
        });
  }

  prune_before_(iterations);
  std::for_each(
      std::make_move_iterator(iterations.begin()),
      std::make_move_iterator(iterations.end()),
      [&cb](iteration&& i) { std::apply(cb, std::move(i)); });
}

template<typename... Args>
void emit_visitor<Args...>::during_without_interval_(const callback& cb,
    const std::optional<time_point>& tr_end) {
  for (auto itp = iteration_tp_();
      itp.has_value() && (!tr_end.has_value() || itp < tr_end.value());
      itp = iteration_tp_()) {
    emit_without_interval_(cb);
  }
}

template<typename... Args>
void emit_visitor<Args...>::after_without_interval_(const callback& cb) {
  pruning_vector iterations;
  for (auto itp = iteration_tp_();
      itp.has_value();
      itp = iteration_tp_()) {
    emit_without_interval_(
        [&iterations](auto&&... args) {
          iterations.emplace_back(std::forward<std::decay_t<decltype(args)>>(args)...);
        });
  }

  prune_after_(iterations);
  std::for_each(
      std::make_move_iterator(iterations.begin()),
      std::make_move_iterator(iterations.end()),
      [&cb](iteration&& i) { std::apply(cb, std::move(i)); });
}

template<typename... Args>
void emit_visitor<Args...>::emit_without_interval_(const callback& cb) {
  using namespace std::placeholders;

  // Extract to-be-emitted value.
  iteration to_be_emitted = iteration_value_();
  // Merge in everything with the same time point.
  while (std::get<0>(to_be_emitted) == iteration_tp_())
    iteration_value_(std::bind(merge_, std::ref(to_be_emitted), _1));
  // Emit.
  std::apply(cb, std::move(to_be_emitted));
}


}} /* namespace monsoon::history */

#endif /* EMIT_VISITOR_INL_H */
