#ifndef MONSOON_MEMOID_H
#define MONSOON_MEMOID_H

#include <functional>
#include <mutex>
#include <optional>

namespace monsoon {


template<typename T>
class memoid {
 public:
  memoid(std::function<T()> fn)
  : fn_(fn)
  {}

  memoid(const memoid&) = delete;
  memoid(memoid&&) = delete;
  memoid& operator=(const memoid&) = delete;
  memoid& operator=(memoid&&) = delete;

  auto get() const
  -> T& {
    std::lock_guard<std::mutex> lck{ mtx_ };
    if (!value_.has_value()) value_.emplace(fn_());
    return *value_;
  }

  auto operator*() const
  -> T& {
    return get();
  }

  auto operator->() const
  -> T* {
    return &get();
  }

  auto reset() -> void {
    std::lock_guard<std::mutex> lck{ mtx_ };
    value_.reset();
  }

 private:
  mutable std::optional<T> value_;
  mutable std::mutex mtx_;
  std::function<T()> fn_;
};


} /* namespace monsoon */

#endif /* MONSOON_MEMOID_H */
