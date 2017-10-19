#ifndef MONSOON_OBJPIPE_PIPE_H
#define MONSOON_OBJPIPE_PIPE_H

#include <monsoon/objpipe/pipe.h>
#include <monsoon/objpipe/errc.h>
#include <condition_variable>
#include <mutex>

namespace monsoon {
namespace pipe {


template<typename T>
class interlock_pipe
: public pipe_impl<T>
{
 public:
  using value_type = typename pipe_impl<T>::value_type;
  using reference = typename pipe_impl<T>::reference;
  using pointer = typename pipe_impl<T>::pointer;
  using rvalue_reference = typename pipe_impl<T>::rvalue_reference;
  using const_reference = typename pipe_impl<T>::const_reference;

  bool empty() const override {
    std::lock_guard<std::mutex> lck{ mtx_ };
    return !in_avail_();
  }

  reference front() const override {
    std::unique_lock<std::mutex> lck{ mtx_ };
    reader_.wait(lck, [this]() { return this->in_avail_(); });
    return *offered_;
  }

  void pop_front() override {
    std::unique_lock<std::mutex> lck{ mtx_ };
    reader_.wait(lck, [this]() { return this->in_avail_(); });
    offered_ = nullptr;

    lck.unlock();
    writer_.notify_one();
    writer_release_.notify_one();
  }

  value_type pull() override {
    std::unique_lock<std::mutex> lck{ mtx_ };
    reader_.wait(lck, [this]() { return this->in_avail_(); });
    value_type v = std::move_if_noexcept(*offered_); // May throw

    offered_ = nullptr;
    lck.unlock();
    if (is_output_connected())
      writer_.notify_one();
    else
      writer_.notify_all();
    return v;
  }

  void push(rvalue_reference v, std::error_condition& ec) override {
    ec = objpipe_errc::success;

    std::unique_lock<std::mutex> lck{ mtx_ };
    writer_.wait(lck, [this]() { return this->out_avail_(ec) && !ec; });
    if (ec) return;

    offered_ = &v;
    reader_.notify_one();
    writer_release_.wait(
        lck,
        [this, &v, &ec]() {
          return this->out_completed_(&v, ec) && !ec;
        });

    if (offered_ == &v) {
      assert(ec);
      offered_ = nullptr;
      writer_.notify_one();
    }
  }

  void push(const_reference v, std::error_condition& ec) override {
    if constexpr(std::is_const_v<value_type>) {
      std::unique_lock<std::mutex> lck{ mtx_ };
      writer_.wait(lck, [this, &ec]() { return this->out_avail_(ec) && !ec; });
      if (ec) return;
      offered_ = &v;
      reader_.notify_one();
      writer_release_.wait(
          lck,
          [this, &v, &ec]() {
            return this->out_completed_(&v, ec) && !ec;
          });

      if (offered_ == &v) {
        assert(ec);
        offered_ = nullptr;
        writer_.notify_one();
      }
    } else {
      push(value_type(v), ec); // Push a copy.
    }
  }

 private:
  void signal_in_close_() override {
    writer_.notify_all();
  }

  void signal_out_close_() override {
    reader_.notify_all();
  }

  bool in_avail_(std::error_condition& ec) const {
    ec = objpipe_errc::success;
    if (offered_ != nullptr) return true;
    if (!is_output_connected()) ec = objpipe_errc::closed;
    return false;
  }

  void wait_in_avail_(std::unique_lock<std::mutex>& lck,
      std::error_condition& ec) {
    assert(lck.mutex() == &mtx_);
    assert(lck.owns_lock());

    reader_.wait(
        lck,
        [this, &ec]() {
          if (offered_ != nullptr) return true;
          if (!is_output_connected()) ec = objpipe_errc::closed;
          return bool(ec);
        }
  }

  bool out_avail_(std::error_condition& ec) const {
    ec = objpipe_errc::success;
    if (!is_output_connected()) ec = objpipe_errc::closed;
    return (offered_ == nullptr);
  }

  bool out_completed_(pointer vptr, std::error_condition& ec) {
    ec = objpipe_errc::success;
    if (offered_ != vptr) return true;
    if (!is_output_connected()) ec = objpipe_errc::closed;
    return false;
  }

  mutable std::mutex mtx_;
  mutable std::condition_variable writer_, reader_, writer_release_;
  pointer offered_ = nullptr;
};


}} /* namespace monsoon::pipe */

#endif /* MONSOON_OBJPIPE_PIPE_H */
