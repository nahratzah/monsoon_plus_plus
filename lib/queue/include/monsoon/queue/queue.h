#ifndef MONSOON_QUEUE_QUEUE_H
#define MONSOON_QUEUE_QUEUE_H

#include <monsoon/queue/queue_export_.h>
#include <memory>
#include <type_traits>

namespace monsoon {
namespace queue {


template<typename> class queue_impl;
class queue_in_release;
class queue_out_release;

/** Queue input side, where items are pushed into the queue. */
template<typename T>
class queue_in {
 public:
  using value_type = T;
  using reference = std::add_lvalue_reference_t<value_type>;

  template<typename Args> void emplace(Args&&...);

  void push(std::add_rvalue_reference_t<value_type>);
  void push(reference);
  template<typename... Args> void emplace(Args&&...);

 private:
  std::unique_ptr<queue_impl<T>, queue_in_release> impl_;
};

/** Queue output side, where items emerge from the queue. */
template<typename T>
class queue_out {
 public:
  using value_type = T;
  using reference = std::add_lvalue_reference_t<value_type>;

  template<typename Fn>
  queue_out filter(Fn&&) &&;
  template<typename Fn>
  queue_out map(Fn&&) &&;

  explicit operator bool() const noexcept;

  reference front() const;
  void pop_front();
  value_type retrieve_front();

 private:
  std::unique_ptr<queue_impl<T>, queue_out_release> impl_;
};


}} /* namespace monsoon */

#endif /* MONSOON_QUEUE_QUEUE_H */
