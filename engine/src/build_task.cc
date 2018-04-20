#include <monsoon/build_task.h>
#include <monsoon/time_series_value.h>
#include <monsoon/time_series.h>
#include <objpipe/of.h>
#include <objpipe/interlock.h>
#include <objpipe/push_policies.h>
#include <algorithm>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace monsoon {
namespace {


auto make_time_series_(const collector::collection& c) -> time_series {
  std::unordered_map<group_name, time_series_value> tsv_map;
  std::for_each(c.elements.begin(), c.elements.end(),
      [&tsv_map](const collector::collection_element& e) {
#if __cplusplus >= 201703
        time_series_value& tsv =
            tsv_map.try_emplace(e.group, e.group).first->second;
#else
        time_series_value& tsv = tsv_map.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(e.group),
            std::forward_as_tuple(e.group)
            ).first->second;
#endif
        tsv.metrics()[e.metric] = e.value;
      });
  auto ts_elems = objpipe::of(std::move(tsv_map))
      .iterate()
      .select_second();

  return time_series(c.tp, ts_elems.begin(), ts_elems.end());
}

class history_multiplexer {
 public:
  explicit history_multiplexer(const std::vector<std::shared_ptr<collect_history>>& histories)
  : histories_(histories)
  {}

  auto operator()(const collector::collection& c) const -> void {
    const time_series ts = make_time_series_(c);

    std::for_each(histories_.begin(), histories_.end(),
        [&ts](const std::shared_ptr<collect_history>& h) {
          h->push_back(ts);
        });
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
          .transform(&collector_metric_source::collection_to_msemit_);
    }
    return {};
  }

  auto attach_history(const history_multiplexer& h) -> void {
    class wrapper {
     public:
      wrapper(history_multiplexer hm)
      : hm_(std::move(hm))
      {}

      auto operator()(const collector::collection& c) {
        hm_(c);
        return objpipe::objpipe_errc::success;
      }

      auto push_exception(const std::exception_ptr& ex) noexcept {
        /* Discard exception. */
      }

     private:
      history_multiplexer hm_;
    };

    sink_.new_pipe()
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

  ///\brief Convert collection to metric_source::emit_type.
  static auto collection_to_msemit_(const collector::collection& c) -> metric_source::emit_type;

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
