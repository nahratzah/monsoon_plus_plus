#include <monsoon/instrumentation.h>
#include <atomic>
#include <string>
#include <vector>
#include <unordered_map>
#include <string_view>
#include <memory>
#include <shared_mutex>
#include <monsoon/group_name.h>
#include <monsoon/simple_group.h>
#include <monsoon/tags.h>
#include <monsoon/metric_name.h>
#include <instrumentation/path.h>
#include <instrumentation/tags.h>
#include <instrumentation/counter.h>
#include <instrumentation/gauge.h>
#include <instrumentation/string.h>

namespace monsoon {
namespace {


class engine
: public instrumentation::engine_intf
{
  private:
  class counter
  : public instrumentation::counter_intf
  {
    public:
    void do_inc(double d) noexcept override {
      double expect = v_.load(std::memory_order_relaxed);
      while (!v_.compare_exchange_weak(expect, expect + d, std::memory_order_relaxed, std::memory_order_relaxed)) {
        // skip
      }
    }

    auto get() const -> metric_value {
      return metric_value(v_.load(std::memory_order_relaxed));
    }

    private:
    std::atomic<double> v_;
  };

  class gauge
  : public instrumentation::gauge_intf
  {
    public:
    void do_inc(double d) noexcept override {
      double expect = v_.load(std::memory_order_relaxed);
      while (!v_.compare_exchange_weak(expect, expect + d, std::memory_order_relaxed, std::memory_order_relaxed)) {
        // skip
      }
    }

    void do_set(double d) noexcept override {
      v_.store(d, std::memory_order_relaxed);
    }

    auto get() const -> metric_value {
      return metric_value(v_.load(std::memory_order_relaxed));
    }

    private:
    std::atomic<double> v_;
  };

  class string
  : public instrumentation::string_intf
  {
    public:
    void do_set(std::string_view s) noexcept override {
      {
        std::shared_lock<std::shared_mutex> s_lck{ mtx_ };
        s_lck.unlock();
      }

      std::lock_guard<std::shared_mutex> lck{ mtx_ };
      v_.assign(s.begin(), s.end());
    }

    auto get() const -> metric_value {
      std::shared_lock<std::shared_mutex> lck{ mtx_ };
      return metric_value(v_);
    }

    private:
    mutable std::shared_mutex mtx_;
    std::string v_;
  };

  class timing
  : public instrumentation::timing_intf,
    private std::allocator<std::atomic<std::uint64_t>>
  {
    public:
    timing() = delete;
    timing(const timing&) = delete;
    timing(timing&&) = delete;
    timing& operator=(const timing&) = delete;
    timing& operator=(timing&&) = delete;

    timing(duration resolution, std::size_t buckets)
    : size_(buckets),
      resolution_(resolution),
      atoms_(allocate(size_))
    {}

    ~timing() noexcept override {
      deallocate(atoms_, size_);
    }

    void do_add(duration d) noexcept override {
      std::size_t idx = d / resolution_;
      if (idx >= size_) idx = size_ - 1u;

      atoms_[idx].fetch_add(1u, std::memory_order_relaxed);
    }

    auto get() const -> metric_value {
      using tdelta = std::chrono::duration<double>;

      histogram h;
      duration d{0};
      for (std::size_t idx = 0; idx < size_ - 1u; ++idx) {
        h.add(
            histogram::range(tdelta(d).count(), tdelta(d + resolution_).count()),
            atoms_[idx]);
        d += resolution_;
      }

      h.add(
          histogram::range(tdelta(d).count(), std::numeric_limits<double>::infinity()),
          atoms_[size_ - 1u]);

      return metric_value(std::move(h));
    }

    private:
    const std::size_t size_ = 0u;
    const duration resolution_;
    std::atomic<std::uint64_t>*const atoms_ = nullptr;
  };

  class cumulative_timing
  : public instrumentation::timing_intf
  {
    public:
    void do_add(duration d) noexcept override {
      sum_.fetch_add(d.count(), std::memory_order_relaxed);
    }

    auto get() const -> metric_value {
      using tdelta = std::chrono::duration<double>;

      // Duration, in floating-point seconds.
      return metric_value(tdelta(duration(sum_.load(std::memory_order_relaxed))).count());
    }

    private:
    std::atomic<duration::rep> sum_{0u};
  };

  using counter_map = std::unordered_map<group_name, std::shared_ptr<counter>>;
  using gauge_map = std::unordered_map<group_name, std::shared_ptr<gauge>>;
  using string_map = std::unordered_map<group_name, std::shared_ptr<string>>;
  using timing_map = std::unordered_map<group_name, std::shared_ptr<timing>>;
  using cumulative_timing_map = std::unordered_map<group_name, std::shared_ptr<cumulative_timing>>;
  using function_map = std::unordered_map<group_name, std::weak_ptr<std::function<double()>>>;
  using str_function_map = std::unordered_map<group_name, std::weak_ptr<std::function<std::string()>>>;

  static auto make_group_name(instrumentation::path p, instrumentation::tags t) -> group_name {
    std::vector<std::pair<std::string_view, metric_value>> tag_map;
    tag_map.reserve(t.data().size());
    for (const auto& e : t.data()) {
      tag_map.emplace_back(
          e.first,
          std::visit(
              [](const auto& v) -> metric_value {
                return metric_value(v);
              },
              e.second));
    }

    return group_name(simple_group(p.data()), tags(std::move(tag_map)));
  }

  public:
  ~engine() noexcept override = default;

  auto new_counter(instrumentation::path p, instrumentation::tags t) -> std::shared_ptr<instrumentation::counter_intf> override {
    const auto emplace_result = counters_.emplace(
        make_group_name(std::move(p), std::move(t)),
        std::make_shared<counter>());

    gauges_.erase(emplace_result.first->first);
    strings_.erase(emplace_result.first->first);
    timings_.erase(emplace_result.first->first);
    cumulative_timings_.erase(emplace_result.first->first);
    functions_.erase(emplace_result.first->first);
    str_functions_.erase(emplace_result.first->first);
    return emplace_result.first->second;
  }

  auto new_gauge(instrumentation::path p, instrumentation::tags t) -> std::shared_ptr<instrumentation::gauge_intf> override {
    const auto emplace_result = gauges_.emplace(
        make_group_name(std::move(p), std::move(t)),
        std::make_shared<gauge>());

    counters_.erase(emplace_result.first->first);
    strings_.erase(emplace_result.first->first);
    timings_.erase(emplace_result.first->first);
    cumulative_timings_.erase(emplace_result.first->first);
    functions_.erase(emplace_result.first->first);
    str_functions_.erase(emplace_result.first->first);
    return emplace_result.first->second;
  }

  auto new_string(instrumentation::path p, instrumentation::tags t) -> std::shared_ptr<instrumentation::string_intf> override {
    const auto emplace_result = strings_.emplace(
        make_group_name(std::move(p), std::move(t)),
        std::make_shared<string>());

    counters_.erase(emplace_result.first->first);
    gauges_.erase(emplace_result.first->first);
    timings_.erase(emplace_result.first->first);
    cumulative_timings_.erase(emplace_result.first->first);
    functions_.erase(emplace_result.first->first);
    str_functions_.erase(emplace_result.first->first);
    return emplace_result.first->second;
  }

  auto new_timing(instrumentation::path p, instrumentation::tags t, instrumentation::timing_intf::duration resolution, std::size_t buckets) -> std::shared_ptr<instrumentation::timing_intf> override {
    const auto emplace_result = timings_.emplace(
        make_group_name(std::move(p), std::move(t)),
        std::make_shared<timing>(resolution, buckets));

    counters_.erase(emplace_result.first->first);
    gauges_.erase(emplace_result.first->first);
    strings_.erase(emplace_result.first->first);
    cumulative_timings_.erase(emplace_result.first->first);
    functions_.erase(emplace_result.first->first);
    str_functions_.erase(emplace_result.first->first);
    return emplace_result.first->second;
  }

  auto new_cumulative_timing(instrumentation::path p, instrumentation::tags t) -> std::shared_ptr<instrumentation::timing_intf> override {
    const auto emplace_result = cumulative_timings_.emplace(
        make_group_name(std::move(p), std::move(t)),
        std::make_shared<cumulative_timing>());

    counters_.erase(emplace_result.first->first);
    gauges_.erase(emplace_result.first->first);
    strings_.erase(emplace_result.first->first);
    timings_.erase(emplace_result.first->first);
    functions_.erase(emplace_result.first->first);
    str_functions_.erase(emplace_result.first->first);
    return emplace_result.first->second;
  }

  auto new_counter_cb(instrumentation::path p, instrumentation::tags t, std::function<double()> cb) -> std::shared_ptr<void> override {
    const auto key = make_group_name(std::move(p), std::move(t));
    auto pointer = std::make_shared<std::function<double()>>(std::move(cb));
    functions_[key] = pointer;

    counters_.erase(key);
    gauges_.erase(key);
    strings_.erase(key);
    timings_.erase(key);
    cumulative_timings_.erase(key);
    str_functions_.erase(key);
    return pointer;
  }

  auto new_gauge_cb(instrumentation::path p, instrumentation::tags t, std::function<double()> cb) -> std::shared_ptr<void> override {
    const auto key = make_group_name(std::move(p), std::move(t));
    auto pointer = std::make_shared<std::function<double()>>(std::move(cb));
    functions_[key] = pointer;

    counters_.erase(key);
    gauges_.erase(key);
    strings_.erase(key);
    timings_.erase(key);
    cumulative_timings_.erase(key);
    str_functions_.erase(key);
    return pointer;
  }

  auto new_string_cb(instrumentation::path p, instrumentation::tags t, std::function<std::string()> cb) -> std::shared_ptr<void> override {
    const auto key = make_group_name(std::move(p), std::move(t));
    auto pointer = std::make_shared<std::function<std::string()>>(std::move(cb));
    str_functions_[key] = pointer;

    counters_.erase(key);
    gauges_.erase(key);
    strings_.erase(key);
    timings_.erase(key);
    cumulative_timings_.erase(key);
    functions_.erase(key);
    return pointer;
  }

  auto collect() -> collector::collection {
    std::vector<collector::collection_element> elements;
    elements.reserve(counters_.size() + gauges_.size() + strings_.size() + functions_.size() + str_functions_.size());

    for (const auto& e : counters_) elements.emplace_back(e.first, metric_name(), e.second->get());
    for (const auto& e : gauges_) elements.emplace_back(e.first, metric_name(), e.second->get());
    for (const auto& e : strings_) elements.emplace_back(e.first, metric_name(), e.second->get());
    for (const auto& e : timings_) elements.emplace_back(e.first, metric_name(), e.second->get());
    for (const auto& weak_e : functions_) {
      const auto e = weak_e.second.lock();
      auto mv = (e && *e) ? metric_value((*e)()) : metric_value();
      elements.emplace_back(weak_e.first, metric_name(), std::move(mv));
    }
    for (const auto& weak_e : str_functions_) {
      const auto e = weak_e.second.lock();
      auto mv = (e && *e) ? metric_value((*e)()) : metric_value();
      elements.emplace_back(weak_e.first, metric_name(), std::move(mv));
    }

    return collector::collection(
        time_point::now(),
        std::move(elements),
        true);
  }

  private:
  counter_map counters_;
  gauge_map gauges_;
  string_map strings_;
  timing_map timings_;
  cumulative_timing_map cumulative_timings_;
  function_map functions_;
  str_function_map str_functions_;
};


auto singleton() -> const std::shared_ptr<engine>& {
  static const auto impl = std::make_shared<engine>();
  return impl;
}


} /* namespace monsoon::collectors::<unnamed> */


instrumentation::engine monsoon_instrumentation() {
  return instrumentation::engine(singleton());
}

collector::collection monsoon_instrumentation_collect() {
  return singleton()->collect();
}


} /* namespace monsoon */
