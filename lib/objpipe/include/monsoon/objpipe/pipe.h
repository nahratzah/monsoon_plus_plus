#ifndef MONSOON_OBJPIPE_PIPE_H
#define MONSOON_OBJPIPE_PIPE_H

#include <monsoon/objpipe/pipe_export_.h>
#include <memory>
#include <iterator>
#include <type_traits>
#include <cstddef>
#include <future>
#include <optional>
#include <atomic>

namespace monsoon {
namespace pipe {


class base_pipe_impl {
  friend class pipe_in_release;
  friend class pipe_out_release;

 protected:
  base_pipe_impl() noexcept {}
  base_pipe_impl(const base_pipe_impl&) = delete;
  base_pipe_impl& operator=(const base_pipe_impl&) = delete;
  base_pipe_impl(base_pipe_impl&&) noexcept {}
  base_pipe_impl& operator=(base_pipe_impl&&) noexcept { return *this; }

 public:
  virtual ~base_pipe_impl() noexcept = default;

  virtual bool empty() const = 0;
  bool is_input_connected() const noexcept;
  bool is_output_connected() const noexcept;

 private:
  virtual void signal_in_close_();
  virtual void signal_out_close_();

  std::atomic<std::uint32_t>
      input_refcount_{0u},
      output_refcount_{0u},
      refcount_{0u};
};

template<typename T>
class pipe_impl
: public base_pipe_impl
{
 public:
  static_assert(!std::is_reference_v<T>,
      "References are not permitted in object pipes.");

  using value_type = T;
  using reference = std::add_lvalue_reference_t<value_type>;
  using rvalue_reference = std::add_rvalue_reference_t<value_type>;
  using const_reference =
      std::add_lvalue_reference_t<std::add_const_t<value_type>>;
  using pointer = std::add_pointer_t<value_type>;

  virtual reference front() const = 0;
  virtual void pop_front() = 0;
  virtual value_type pull() = 0;
  virtual void push(rvalue_reference) = 0;
  virtual void push(const_reference) = 0;
};

class pipe_in_release {
 public:
  void operator()(base_pipe_impl* ptr) const {
    if (ptr == nullptr) return;
    if (ptr->input_refcount_.fetch_sub(1u, std::memory_order_acq_rel) == 1u)
      ptr->signal_in_close_();
    if (ptr->refcount_.fetch_sub(1u, std::memory_order_acq_rel) == 1u)
      delete ptr;
  }
};

class pipe_out_release {
 public:
  void operator()(base_pipe_impl* ptr) const {
    if (ptr == nullptr) return;
    if (ptr->output_refcount_.fetch_sub(1u, std::memory_order_acq_rel) == 1u
        && ptr->empty())
      ptr->signal_out_close_();
    if (ptr->refcount_.fetch_sub(1u, std::memory_order_acq_rel) == 1u)
      delete ptr;
  }
};

template<typename> class pipe_out_iterator;

/** Queue input side, where items are pushed into the pipe. */
template<typename T>
class pipe_in {
 public:
  using value_type = T;
  using reference = std::add_lvalue_reference_t<value_type>;

  template<typename Args> void emplace(Args&&...);

  void push(std::add_rvalue_reference_t<value_type>);
  void push(reference);
  template<typename... Args> void emplace(Args&&...);

 private:
  std::unique_ptr<pipe_impl<T>, pipe_in_release> impl_;
};

/** Queue output side, where items emerge from the pipe. */
template<typename T>
class pipe_out {
 public:
  using value_type = T;
  using reference = std::add_lvalue_reference_t<value_type>;
  using rvalue_reference = std::add_rvalue_reference_t<value_type>;
  using iterator = pipe_out_iterator<T>;

  template<typename Fn>
  auto filter(Fn&&) && -> pipe_out;
#if __cplusplus >= 201703
  template<typename Fn>
  auto map(Fn&&) && -> pipe_out<std::decay_t<std::invoke_result_t<std::decay_t<Fn>, rvalue_reference>>>;
#else
  template<typename Fn>
  auto map(Fn&&) &&
      -> pipe_out<std::decay_t<decltype(
          std::invoke(
              std::declval<std::add_lvalue_reference_t<std::decay_t<Fn>>>(),
              std::declval<rvalue_reference>()))>>;
#endif
  template<typename Fn>
  std::future<void> sink(Fn&&) &&;
  template<typename Fn>
  std::future<void> sink(std::launch, Fn&&) &&;

  explicit operator bool() const noexcept;

  bool empty() const;
  reference front() const;
  void pop_front();
  value_type pull();
  std::optional<value_type> pull(std::nothrow_t);
  std::optional<value_type> try_pull();

  iterator begin() { return iterator(this); }
  iterator end() { return {}; }

 private:
  std::unique_ptr<pipe_impl<T>, pipe_out_release> impl_;
};

template<typename T>
class pipe_out_iterator {
 public:
  using iterator_category = std::input_iterator_tag;
  using value_type = typename pipe_out<T>::value_type;
  using reference = std::add_rvalue_reference_t<value_type>;
  using pointer = std::add_pointer_t<std::remove_reference_t<reference>>;
  using difference_type = std::ptrdiff_t;

  explicit pipe_out_iterator(pipe_out<T>* q) noexcept : q_(q) {}

  reference operator*() const;
  pointer operator->() const;
  pipe_out_iterator& operator++();
  void operator++(int);

  bool operator==(const pipe_out_iterator&) const noexcept;
  bool operator!=(const pipe_out_iterator&) const noexcept;

 private:
  pipe_out<T>* q_ = nullptr;
};


}} /* namespace monsoon::pipe */

#endif /* MONSOON_OBJPIPE_PIPE_H */
