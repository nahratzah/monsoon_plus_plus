#ifndef MONSOON_OBJPIPE_COLLECTION_PIPE_H
#define MONSOON_OBJPIPE_COLLECTION_PIPE_H

#include <iterator>
#include <cassert>

namespace monsoon {
namespace pipe {


template<typename Collection>
class collection_pipe
: public queue_impl<typename std::iterator_traits<typename collection::iterator>::value_type>
{
 private:
  using parent_type =
      queue_impl<typename std::iterator_traits<typename collection::iterator>::value_type>;

 public:
  using collection = Collection;
  using iterator = typename collection::iterator;
  using value_type = typename parent_type::value_type;
  using reference = typename parent_type::reference;
  using rvalue_reference = typename parent_type::rvalue_reference;
  using const_reference = typename parent_type::const_reference;

  collection_pipe(collection&& c)
  : c_(std::move(c)),
    i_(begin(c_)),
    e_(end(c_))
  {}

  bool empty() const override {
    return i_ != e_;
  }

  reference front() const override {
    assert(i_ != e_);
    return *i_;
  }

  void pop_front() override {
    assert(i_ != e_);
    ++i_;
  }

  value_type pull() override {
    value_type v = std::move(*i_);
    ++i_;
    return v;
  }

  void push(rvalue_reference v) {
    assert(false); // No implementation.
  }

  void push(const_reference v) {
    assert(false); // No implementation.
  }

 private:
  void signal_in_close_() override {};
  void signal_out_close_() override {};

  Collection c_;
  iterator i_, e_;
};


}} /* namespace monsoon::pipe */

#endif /* MONSOON_OBJPIPE_COLLECTION_PIPE_H */
