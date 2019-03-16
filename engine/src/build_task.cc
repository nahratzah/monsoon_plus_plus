#include <monsoon/build_task.h>
#include <monsoon/time_series_value.h>
#include <monsoon/time_series.h>
#include <objpipe/interlock.h>
#include <objpipe/push_policies.h>
#include <algorithm>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#ifndef NDEBUG
# include <iostream>
#endif

namespace monsoon {
namespace {


#ifdef NDEBUG
template<typename CollectorPipe>
auto maybe_perform_validation_(
    const collector::names_set& names,
    CollectorPipe&& pipe)
-> CollectorPipe&& {
  return std::forward<CollectorPipe>(pipe);
}
#else
template<typename CollectorPipe>
auto maybe_perform_validation_(
    const collector::names_set& names,
    CollectorPipe&& pipe)
-> auto {
  return std::forward<CollectorPipe>(pipe)
      .peek(
          [names](const collector::collection& c) {
            for (const auto& elem : c.elements) {
              // Try to match name against set of literal names.
              bool found = (names.known.count(
                      std::forward_as_tuple(
                          elem.group,
                          elem.metric)) != 0);

              // Try to match name against any of the matchers.
              if (!found) {
                for (const auto& matchers : names.unknown) {
                  found = (std::get<0>(matchers)(elem.group.get_path())
                      && std::get<1>(matchers)(elem.group.get_tags())
                      && std::get<2>(matchers)(elem.metric));
                  if (found) break;
                }
              }

              // Report name violation.
              if (!found) {
                std::cerr << "BUG: collector::provides() failed to account for "
                    << elem.group << "::" << elem.metric << std::endl;
              }
            }
          });
}
#endif


class history_multiplexer {
 public:
  explicit history_multiplexer(const std::vector<std::shared_ptr<collect_history>>& histories)
  : histories_(histories)
  {}

  auto operator()(const metric_source::emit_type& e) const -> void {
    if (std::holds_alternative<metric_source::metric_emit>(e)) {
      const auto& emit = std::get<metric_source::metric_emit>(e);

      std::for_each(
          histories_.begin(), histories_.end(),
          [&emit](const std::shared_ptr<collect_history>& h) {
            h->push_back(emit);
          });
    }
  }

 private:
  std::vector<std::shared_ptr<collect_history>> histories_;
};


class time_multiplexer {
 private:
  class sink {
   public:
    sink() = default;

    auto new_pipe()
    -> objpipe::interlock_reader<time_point> {
      objpipe::interlock_reader<time_point> r;
      objpipe::interlock_writer<time_point> w;
      std::tie(r, w) = objpipe::new_interlock<time_point>();

      sinks_.push_back(std::move(w));
      return r;
    }

    auto operator()(time_point tp)
    -> objpipe::objpipe_errc {
      using objpipe::objpipe_errc;

      objpipe_errc rv = objpipe_errc::success;

      auto sink_iter = sinks_.begin();
      while (sink_iter != sinks_.end()) {
        objpipe_errc e = (*sink_iter)(tp);

        if (e != objpipe_errc::success) {
          sink_iter = sinks_.erase(sink_iter);
          if (e != objpipe_errc::closed)
            rv = e;
        } else {
          ++sink_iter;
        }
      }

      if (rv == objpipe_errc::success)
        return (sinks_.empty() ? objpipe_errc::closed : objpipe_errc::success);
      return rv;
    }

    auto push_exception(const std::exception_ptr& exptr)
    noexcept
    -> void {
      std::for_each(
          sinks_.begin(), sinks_.end(),
          [exptr](auto& sink) { sink.push_exception(exptr); });
    }

   private:
    std::vector<objpipe::interlock_writer<time_point>> sinks_;
  };

 public:
  auto new_pipe()
  -> objpipe::interlock_reader<time_point> {
    return sink_.new_pipe();
  }

  auto commit() &&
  -> objpipe::interlock_writer<time_point> {
    objpipe::interlock_reader<time_point> r;
    objpipe::interlock_writer<time_point> w;
    std::tie(r, w) = objpipe::new_interlock<time_point>();

    std::move(r)
        .async(objpipe::existingthread_push())
        .push(std::move(sink_));
    return w;
  }

 private:
  sink sink_;
};


class collector_metricsource_converter_ {
 private:
  using map_type = std::tuple_element_t<1, metric_source::metric_emit>;

  struct record {
    record() = default;

    record(const collector::collection& c)
    : tp(c.tp)
    {
      this->merge(c);
    }

    auto merge(const collector::collection& c)
    -> void {
      assert(tp == c.tp);

      for (const auto& elem : c.elements) {
#if __cplusplus >= 201703
        data.insert_or_assign(std::make_tuple(elem.group, elem.metric), elem.value);
#else
        data[std::make_tuple(elem.group, elem.metric)] = elem.value;
#endif
      }

      complete |= c.is_complete;
    }

    time_point tp;
    map_type data;
    bool complete;
  };

 public:
  auto accept(const collector::collection& c)
  -> void {
    assert(invariant());

    if (queue_.empty() || queue_.back().tp < c.tp) {
      queue_.emplace_back(c);
    } else if (queue_.back().tp == c.tp) {
      queue_.back().merge(c);
    } else if (queue_.front().tp == c.tp) {
      queue_.front().merge(c);
    } else {
      auto pos = std::lower_bound(
          queue_.begin(), queue_.end(),
          c,
          [](const auto& x, const auto& y) { return x.tp < y.tp; });
      assert(pos == queue_.end() || pos->tp >= c.tp);
      if (pos != queue_.end() && pos->tp == c.tp)
        pos->merge(c);
      else
        queue_.emplace(pos, c);
    }

    assert(invariant());
  }

  auto maybe_emit()
  -> std::optional<metric_source::emit_type> {
    assert(invariant());

    std::optional<metric_source::emit_type> result;
    if (!queue_.empty() && queue_.front().complete) {
      result.emplace(
          std::in_place_type<metric_source::metric_emit>,
          std::move(queue_.front().tp),
          std::move(queue_.front().data));
      queue_.pop_front();

      assert(invariant());
    }

    return result;
  }

  static auto speculative_entries(const collector::collection& c)
  -> std::vector<metric_source::emit_type> {
    std::vector<metric_source::emit_type> result;
    result.reserve(c.elements.size());
    const time_point tp = c.tp;

    std::transform(
        c.elements.begin(), c.elements.end(),
        std::back_inserter(result),
        [tp](const auto& element) -> metric_source::speculative_metric_emit {
          return { tp, element.group, element.metric, element.value };
        });
    return result;
  }

 private:
  ///\brief Invariant: queue must be ordered by time point and may not hold duplicate timepoints.
  auto invariant() const
  -> bool {
    if (queue_.empty()) return true;

    auto pred = queue_.begin();
    auto succ = std::next(pred);
    while (succ != queue_.end()) {
      if (pred->tp >= succ->tp) return false;
      ++pred;
      ++succ;
    }
    return true;
  }

  std::deque<record> queue_;
};

template<typename Acceptor>
class collector_metricsource_push_ {
 public:
  collector_metricsource_push_(Acceptor&& dst)
  : dst_(std::move(dst))
  {}

  auto operator()(const collector::collection& c)
  -> objpipe::objpipe_errc {
    state_.accept(c);

    auto emit = state_.maybe_emit();
    if (!emit.has_value()) {
      auto se = collector_metricsource_converter_::speculative_entries(c);
      for (auto& x : se) {
        const objpipe::objpipe_errc e = dst_(std::move(x));
        if (e != objpipe::objpipe_errc::success)
          return e;
      }
    } else {
      do {
        const objpipe::objpipe_errc e = dst_(*std::move(emit));
        if (e != objpipe::objpipe_errc::success) return e;
        emit = state_.maybe_emit();
      } while (emit.has_value());
    }

    return objpipe::objpipe_errc::success;
  }

  auto push_exception(std::exception_ptr exptr)
  noexcept
  -> void {
    dst_.push_exception(std::move(exptr));
  }

 private:
  collector_metricsource_converter_ state_;
  Acceptor dst_;
};

template<typename Source>
class collector_metricsource_pipe_ {
 private:
  using transport_type = objpipe::detail::transport<metric_source::emit_type&&>;
  using objpipe_errc = objpipe::objpipe_errc;

 public:
  collector_metricsource_pipe_(Source&& src)
  : src_(std::move(src))
  {}

  collector_metricsource_pipe_(const collector_metricsource_pipe_&) = delete;
  collector_metricsource_pipe_(collector_metricsource_pipe_&&) = default;
  auto operator=(const collector_metricsource_pipe_&) -> collector_metricsource_pipe_& = delete;
  auto operator=(collector_metricsource_pipe_&&) -> collector_metricsource_pipe_& = delete;

  auto is_pullable() noexcept
  -> bool {
    return !pending_.empty() || src_.is_pullable();
  }

  auto wait()
  -> objpipe_errc {
    if (!pending_.empty()) return objpipe_errc::success;
    return front().errc();
  }

  auto front()
  -> transport_type {
    for (;;) {
      if (!pending_.empty())
        return transport_type(std::in_place_index<0>, std::move(pending_.front()));

      auto src_val = objpipe::detail::adapt::raw_pull(src_);
      if (src_val.errc() != objpipe_errc::success)
        return transport_type(std::in_place_index<1>, src_val.errc());
      state_.accept(src_val.ref());

      auto emit = state_.maybe_emit();
      if (emit.has_value()) {
        pending_.push_back(*std::move(emit));
      } else {
        auto se = collector_metricsource_converter_::speculative_entries(src_val.ref());
        pending_.insert(
            pending_.end(),
            std::make_move_iterator(se.begin()),
            std::make_move_iterator(se.end()));
      }
    }
  }

  auto pop_front()
  -> objpipe_errc {
    if (pending_.empty()) {
      objpipe_errc e = front().errc();
      if (e != objpipe_errc::success) return e;
    }

    pending_.pop_front();
    return objpipe_errc::success;
  }

  template<bool Enable = objpipe::detail::adapt::has_ioc_push<Source, objpipe::existingthread_push>>
  auto can_push(const objpipe::existingthread_push& tag) const
  noexcept
  -> std::enable_if_t<Enable, bool> {
    return src_.can_push(tag);
  }

  template<bool Enable = objpipe::detail::adapt::has_ioc_push<Source, objpipe::singlethread_push>>
  auto can_push(const objpipe::singlethread_push& tag) const
  noexcept
  -> std::enable_if_t<Enable, bool> {
    return src_.can_push(tag);
  }

  template<typename Acceptor, bool Enable = objpipe::detail::adapt::has_ioc_push<Source, objpipe::existingthread_push>>
  auto ioc_push(const objpipe::existingthread_push& tag, Acceptor&& acceptor) &&
  noexcept
  -> std::enable_if_t<Enable> {
    using impl = collector_metricsource_push_<std::decay_t<Acceptor>>;

    std::move(src_).ioc_push(tag, impl(std::forward<Acceptor>(acceptor)));
  }

  template<typename Acceptor, bool Enable = objpipe::detail::adapt::has_ioc_push<Source, objpipe::singlethread_push>>
  auto ioc_push(const objpipe::singlethread_push& tag, Acceptor&& acceptor) &&
  noexcept
  -> std::enable_if_t<Enable> {
    using impl = collector_metricsource_push_<std::decay_t<Acceptor>>;

    std::move(src_).ioc_push(tag, impl(std::forward<Acceptor>(acceptor)));
  }

 private:
  collector_metricsource_converter_ state_;
  Source src_;
  std::deque<metric_source::emit_type> pending_;
};


class collector_metric_source {
 private:
  class sink {
   public:
    sink() = default;

    auto operator()(const collector::collection& c) -> objpipe::objpipe_errc {
      using objpipe::objpipe_errc;
      objpipe_errc rv = objpipe_errc::success;

      auto sink_iter = sinks_.begin();
      while (sink_iter != sinks_.end()) {
        objpipe::interlock_writer<collector::collection>& sink = *sink_iter;
        objpipe_errc e = sink(c);

        // Erase sinks that are closed/bad.
        if (e != objpipe_errc::success) {
          sink_iter = sinks_.erase(sink_iter);
          if (rv != objpipe_errc::success && e != objpipe_errc::closed)
            rv = e;
        } else {
          ++sink_iter;
        }
      }

      if (rv == objpipe_errc::success)
        return (sinks_.empty() ? objpipe_errc::closed : objpipe_errc::success);
      return rv;
    }

    auto push_exception(std::exception_ptr ex) noexcept -> void {
      for (objpipe::interlock_writer<collector::collection>& sink : sinks_)
        sink.push_exception(ex);
      sinks_.clear();
    }

    auto new_pipe() -> objpipe::interlock_reader<collector::collection> {
      objpipe::interlock_reader<collector::collection> r;
      objpipe::interlock_writer<collector::collection> w;
      std::tie(r, w) = objpipe::new_interlock<collector::collection>();
      sinks_.emplace_back(std::move(w));
      return r;
    }

   private:
    std::vector<objpipe::interlock_writer<collector::collection>> sinks_;
  };

  ///\brief Convert collection to metric_source::emit_type.
  template<typename CollectionPipe>
  static auto collection_to_msemit_(CollectionPipe&& pipe) {
    using raw_pipe_t = objpipe::detail::adapter_underlying_type_t<std::decay_t<CollectionPipe>>;
    using result_t = collector_metricsource_pipe_<raw_pipe_t>;
    return objpipe::detail::adapter(result_t(std::forward<CollectionPipe>(pipe).underlying()));
  }

 public:
  collector_metric_source(const collector& c)
  : c_(&c)
  {}

  void commit(objpipe::reader<time_point>&& ts_pipe) && {
    std::move(ts_pipe)
        .perform(
            [this](auto&& pipe) {
              return c_->run(std::forward<decltype(pipe)>(pipe));
            })
        .perform(
            [this](auto&& pipe) {
              return maybe_perform_validation_(c_->provides(), std::forward<decltype(pipe)>(pipe));
            })
        .async(objpipe::existingthread_push())
        .push(std::move(sink_));
  }

  auto emit(
      path_matcher group_filter,
      tag_matcher tag_filter,
      path_matcher metric_filter)
  -> std::optional<objpipe::reader<metric_source::emit_type>> {
    if (intersects_(group_filter, tag_filter, metric_filter)) {
      return sink_.new_pipe()
          .peek(
              [group_filter, tag_filter, metric_filter](collector::collection& c) -> void {
                filter_collection_elements_(group_filter, tag_filter, metric_filter, c);
              })
          .perform(
              [](auto&& x) {
                return collector_metric_source::collection_to_msemit_(std::forward<decltype(x)>(x));
              });
    }
    return {};
  }

  auto attach_history(const history_multiplexer& h) -> void {
    class wrapper {
     public:
      wrapper(history_multiplexer hm)
      : hm_(std::move(hm))
      {}

      auto operator()(const metric_source::emit_type& c) {
        hm_(c);
        return objpipe::objpipe_errc::success;
      }

      auto push_exception(const std::exception_ptr& ex) noexcept {
        /* Discard exception. */
      }

     private:
      history_multiplexer hm_;
    };

    collection_to_msemit_(sink_.new_pipe())
        .async(objpipe::existingthread_push())
        .push(wrapper(h));
  }

 private:
  ///\brief Remove names from collection, keeping only matching names.
  static auto filter_collection_elements_(
      const path_matcher& group_filter,
      const tag_matcher& tag_filter,
      const path_matcher& metric_filter,
      collector::collection& c)
  -> void {
    c.elements.erase(
        std::remove_if(
            c.elements.begin(),
            c.elements.end(),
            [&group_filter, &tag_filter, &metric_filter](const collector::collection_element& e) -> bool {
              return !group_filter(e.group.get_path())
                  || !tag_filter(e.group.get_tags())
                  || !metric_filter(e.metric);
            }),
        c.elements.end());
  }

  ///\brief Test if the given filters intersect with provided names.
  auto intersects_(
      path_matcher group_filter,
      tag_matcher tag_filter,
      path_matcher metric_filter) const
  -> bool {
    collector::names_set names = c_->provides();

    if (std::any_of(names.known.begin(), names.known.end(),
            [&group_filter, &tag_filter, &metric_filter](const std::tuple<group_name, metric_name>& name) -> bool {
              return group_filter(std::get<0>(name).get_path())
                  && tag_filter(std::get<0>(name).get_tags())
                  && metric_filter(std::get<1>(name));
            }))
      return true;

    if (std::any_of(names.unknown.begin(), names.unknown.end(),
            [&group_filter, &tag_filter, &metric_filter](const std::tuple<path_matcher, tag_matcher, path_matcher>& name) -> bool {
              return has_overlap(group_filter, std::get<0>(name))
                  && has_overlap(tag_filter, std::get<1>(name))
                  && has_overlap(metric_filter, std::get<2>(name));
            }))
      return true;

    return false;
  }

  const collector* c_ = nullptr;
  sink sink_;
};


} /* namespace monsoon::<unnamed> */


auto build_task(
    const configuration& cfg,
    const std::vector<std::shared_ptr<collect_history>>& histories)
-> objpipe::interlock_writer<time_point> {
  time_multiplexer tm;
  std::vector<collector_metric_source> collectors;
  collectors.reserve(cfg.collectors().size());

  // Build collectors.
  std::transform(
      cfg.collectors().begin(),
      cfg.collectors().end(),
      std::back_inserter(collectors),
      [](const auto& cptr) { return collector_metric_source(*cptr); });

  // Attach histories.
  std::for_each(collectors.begin(), collectors.end(),
      [&histories](collector_metric_source& cms) {
        cms.attach_history(history_multiplexer(histories));
      });

  // XXX: handle rules (this will likely affect how histories are attached).

  // Attach time to all collectors.
  for (collector_metric_source& cms : collectors)
    std::move(cms).commit(tm.new_pipe());
  // Return functor.
  return std::move(tm).commit();
}


} /* namespace monsoon */
