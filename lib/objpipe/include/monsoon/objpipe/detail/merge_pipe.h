#ifndef MONSOON_OBJPIPE_DETAIL_MERGE_PIPE_H
#define MONSOON_OBJPIPE_DETAIL_MERGE_PIPE_H

#include <algorithm>
#include <functional>
#include <iterator>
#include <type_traits>
#include <monsoon/objpipe/detail/invocable_.h>

namespace monsoon::objpipe::detail {


///\brief Adapter for source, that maintains a reference to source front.
///\tparam Underlying objpipe source.
template<typename Source>
class merge_queue_elem_ {
 public:
  ///\brief Transport type for get().
  using transport_type = transport<adapt::front_type<Source>>;
  ///\brief Value type of the source.
  using value_type = adapt::value_type<Source>;

  constexpr merge_queue_elem_(merge_queue_elem_&& other)
  noexcept(std::is_nothrow_move_constructible_v<transport_type>
      && std::is_nothrow_move_constructible_v<Source>)
  : front_val_(std::move(other.front_val_)),
    src_(std::move(other.src_))
  {}

  auto operator=(merge_queue_elem_&& other) noexcept
  -> merge_queue_elem_& {
    front_val_ = std::move(front_val_);
    src_ = std::move(src_);
    return *this;
  }

  ///\brief Construct a new source.
  constexpr merge_queue_elem_(Source&& src)
  noexcept(std::is_nothrow_move_constructible_v<Source>)
  : src_(std::move(src))
  {}

  auto is_pullable() const noexcept
  -> bool {
    if (front_val_.has_value())
      return front_val_->errc() != objpipe_errc::closed;
    return src_.is_pullable();
  }

  ///\brief Returns a reference to Source.front().
  ///\note In constrast with Source.front(), repeated invocations are fine.
  auto get()
  -> transport_type& {
    if (!front_val_.has_value())
      front_val_.emplace(src_.front());
    return *front_val_;
  }

  ///\brief Invokes src.pop_front().
  ///\note Consequently, this invalidates the reference from get().
  auto reset()
  noexcept(std::is_nothrow_destructible_v<transport_type>
      && noexcept(std::declval<Source&>().pop_front()))
  -> objpipe_errc {
    assert(front_val_.has_value());
    assert(front_val_->errc() == objpipe_errc::success);

    front_val_.reset();
    return src_.pop_front();
  }

  ///\brief Returns get(), but destructively.
  auto release()
  noexcept(std::is_nothrow_destructible_v<transport_type>
      && std::is_nothrow_move_constructible_v<transport_type>
      && noexcept(std::declval<Source&>().pop_front()))
  -> transport_type&& {
    return std::move(get());
  }

 private:
  std::optional<transport_type> front_val_;
  Source src_;
};

template<typename Source, typename Less>
class merge_pipe_base {
 public:
  using queue_elem = merge_queue_elem_<Source>;
  using transport_type = typename queue_elem::transport_type;
  using value_type = typename queue_elem::value_type;

 private:
  ///\brief Comparator for queue_elem, using the greater() comparison.
  ///\details Intended for heap sorting.
  ///\note This comparator sorts errors to the front, allowing for early bail out.
  class greater {
   public:
    greater() = delete;

    constexpr greater(const Less& less)
    : less_(less)
    {}

    auto operator()(queue_elem& x, queue_elem& y) const
    -> bool {
      transport_type x_front = x.get(),
                     y_front = y.get();

      // Sort errors to the front.
      if (x_front.errc() != objpipe_errc::success
          || y_front.errc() != objpipe_errc::success)
        return x_front.errc() > y_front.errc();

      // Both must have values, so compare those.
      assert(x_front.has_value() && y_front.has_value());
      // Operate on constants.
      const value_type& x_val = static_cast<const value_type&>(x_front.value());
      const value_type& y_val = static_cast<const value_type&>(y_front.value());
      return std::invoke(less_, y_val, x_val);
    }

   private:
    const Less& less_;
  };

  using queue_container_type = std::vector<queue_elem>;

 public:
  template<typename Iter>
  merge_pipe_base(Iter src_begin, Iter src_end, Less&& less)
  : less_(std::move(less))
  {
    std::transform(src_begin, src_end,
        std::back_inserter(data_),
        [](auto&& src) { return std::move(src).underlying(); });
  }

  template<typename Iter>
  merge_pipe_base(Iter src_begin, Iter src_end, const Less& less)
  : less_(less)
  {
    std::transform(src_begin, src_end,
        std::back_inserter(data_),
        [](auto&& src) { return std::move(src).underlying(); });
  }

  auto is_pullable() const noexcept {
    if (need_init_) {
      return std::any_of(
          data_.begin(),
          data_.end(),
          std::mem_fn(&queue_elem::is_pullable));
    }

    const auto data_size = data_.size();
    switch (data_.size()) {
      case 0:
        break;
      default:
        assert(std::all_of(
                data_.begin(),
                data_.end() - 1,
                std::mem_fn(&queue_elem::is_pullable)));
        return true;
      case 1:
        if (data_.back().is_pullable()) return true;
        break;
    }
    return false;
  }

  auto wait()
  noexcept(noexcept(std::declval<queue_elem&>().get()))
  -> objpipe_errc {
    if (need_init_) { // Need to traverse all elements.
      bool all_closed = true;
      objpipe_errc e = objpipe_errc::success;
      for (queue_elem& elem : data_) {
        objpipe_errc elem_e = elem.get().errc();
        if (elem_e != objpipe_errc::closed) {
          all_closed = false;
          if (elem_e > e) e = elem_e;
        }
      }
      return (e == objpipe_errc::success && all_closed
          ? objpipe_errc::closed
          : e);
    }

    // Need to check current and front elements:
    // - current element may have stored error code.
    // - front element is next and may compare higher in next get_front_source() call, if it has an error.
    objpipe_errc e = (data_.empty()
        ? objpipe_errc::closed
        : data_.back().get().errc());
    if (e == objpipe_errc::closed && data_.size() >= 2)
      e = data_.front().get().errc();
    return e;
  }

  auto wait() const
  noexcept(noexcept(std::declval<queue_elem&>().get()))
  -> objpipe_errc {
    return const_cast<merge_pipe_base&>(*this).wait();
  }

  /**
   * \brief Find the front element to be emitted.
   * \details Invalidates previous references to data_ elements.
   *
   * Irrespective if the previous result from get_front_source() was
   * modified, this function may return a difference queue element.
   *
   * \note Address of returned elements for previous invocations may
   * be the same, but this does not indicate they are the same.
   *
   * \returns A pointer to the least queue_elem in data_,
   * that is not empty.
   * If there are no more elements, nullptr is returned.
   */
  auto get_front_source()
  -> queue_elem* {
    // Make data_ be heap sorted in its entirety.
    if (need_init_) {
      std::make_heap(data_.begin(), data_.end(), greater(less_));
      need_init_ = false;
    } else if (!data_.empty()) {
      std::push_heap(data_.begin(), data_.end(), greater(less_));
    }

    // Pop one element from the heap.
    while (!data_.empty()) {
      std::pop_heap(data_.begin(), data_.end(), greater(less_));
      if (data_.back().get().errc() == objpipe_errc::closed)
        data_.pop_back();
      else
        return &data_.back();
    }
    return nullptr;
  }

  auto is_less(const value_type& x, const value_type& y) const
  noexcept(is_nothrow_invocable_v<Less&, const value_type&, const value_type&>)
  -> bool {
    return std::invoke(less_, x, y);
  }

 private:
  /**
   * \brief All sources.
   * \details data_.back() is the element we operate upon, that can be freely modified.
   *
   * The \ref get_queue_front_() method reorders the queue each time it is called.
   * \invariant
   * \code
   * need_init_
   * || data_.empty()
   * || is_heap_sorted(data_.begin(), data_.end() - 1)
   * \endcode
   */
  queue_container_type data_;
  Less less_;
  bool need_init_ = true;
};

template<typename Source, typename Less>
class merge_pipe
: private merge_pipe_base<Source, Less>
{
 private:
  using queue_elem = typename merge_pipe_base<Source, Less>::queue_elem;
  using transport_type = typename merge_pipe_base<Source, Less>::transport_type;

 public:
  using merge_pipe_base<Source, Less>::merge_pipe_base;
  using merge_pipe_base<Source, Less>::operator=;

  using merge_pipe_base<Source, Less>::is_pullable;
  using merge_pipe_base<Source, Less>::wait;

  auto front()
  -> transport_type {
    assert(recent == nullptr);

    recent = this->get_front_source();
    if (recent == nullptr)
      return transport_type(std::in_place_index<1>, objpipe_errc::closed);
    return recent->release();
  }

  auto pop_front()
  -> objpipe_errc {
    if (recent == nullptr) {
      objpipe_errc e = front().errc();
      if (e != objpipe_errc::success)
        return e;
    }
    std::exchange(recent, nullptr)->reset();
    return objpipe_errc::success;
  }

  auto front() const
  -> transport_type {
    return const_cast<merge_pipe&>(*this).front();
  }

 private:
  queue_elem* recent = nullptr;
};

template<typename Type, typename ReduceOp> class do_merge_t;

template<typename Source, typename Less, typename ReduceOp>
class merge_reduce_pipe
: private merge_pipe_base<Source, Less>
{
 private:
  using queue_elem = typename merge_pipe_base<Source, Less>::queue_elem;
  using value_type = typename merge_pipe_base<Source, Less>::value_type;
  using transport_type = transport<value_type>;

 public:
  template<typename Iter, typename LessInit>
  merge_reduce_pipe(Iter src_begin, Iter src_end, LessInit&& less, ReduceOp&& reduce_op)
  : merge_pipe_base<Source, Less>(src_begin, src_end, std::forward<LessInit>(less)),
    do_merge_(std::move(reduce_op))
  {}

  template<typename Iter, typename LessInit>
  merge_reduce_pipe(Iter src_begin, Iter src_end, LessInit&& less, const ReduceOp& reduce_op)
  : merge_pipe_base<Source, Less>(src_begin, src_end, std::forward<LessInit>(less)),
    do_merge_(reduce_op)
  {}

  using merge_pipe_base<Source, Less>::is_pullable;
  using merge_pipe_base<Source, Less>::wait;

  auto front()
  -> transport_type {
    assert(!pending_pop);

    queue_elem* head = this->get_front_source();
    if (head == nullptr)
      return transport_type(std::in_place_index<1>, objpipe_errc::closed);

    transport_type val = head->release();
    if (val.errc() != objpipe_errc::success) return val;
    objpipe_errc e = head->reset();
    if (e != objpipe_errc::success)
      return transport_type(std::in_place_index<1>, e);
    assert(val.has_value());

    // Keep merging in successive values, until less comparison holds.
    for (;;) {
      head = this->get_front_source();
      if (head == nullptr) break;
      auto& head_val = head->get();
      if (head_val.errc() != objpipe_errc::success) break;
      if (this->is_less(val.value(), head_val.value())) break;

      do_merge_(val, head->release());
      e = head->reset();
      if (e != objpipe_errc::success)
        return transport_type(std::in_place_index<1>, e);
    }

    pending_pop = true;
    return val;
  }

  auto pop_front()
  -> objpipe_errc {
    if (!pending_pop) {
      objpipe_errc e = front().errc();
      if (e != objpipe_errc::success)
        return e;
    }

    pending_pop = false;
    return objpipe_errc::success;
  }

  auto front() const
  -> transport_type {
    return const_cast<merge_reduce_pipe&>(*this).front();
  }

 private:
  bool pending_pop = false;
  do_merge_t<value_type, ReduceOp> do_merge_;
};

///\brief Functor that invokes reduce_op on transports of value_type.
///\details The result of the reduction is stored in the first argument to this functor.
template<typename Type, typename ReduceOp>
class do_merge_t {
 private:
  using value_type = Type;
  static_assert(!std::is_const_v<Type> && !std::is_volatile_v<Type> && !std::is_reference_v<Type>,
      "Type must be unqualified.");

  static constexpr bool is_cc_invocable =
      is_invocable_v<const ReduceOp&, const Type&, const Type&>;
  static constexpr bool is_cl_invocable =
      is_invocable_v<const ReduceOp&, const Type&, Type&>;
  static constexpr bool is_cr_invocable =
      is_invocable_v<const ReduceOp&, const Type&, Type&&>;

  static constexpr bool is_lc_invocable =
      is_invocable_v<const ReduceOp&, Type&, const Type&>;
  static constexpr bool is_ll_invocable =
      is_invocable_v<const ReduceOp&, Type&, Type&>;
  static constexpr bool is_lr_invocable =
      is_invocable_v<const ReduceOp&, Type&, Type&&>;

  static constexpr bool is_rc_invocable =
      is_invocable_v<const ReduceOp&, Type&&, const Type&>;
  static constexpr bool is_rl_invocable =
      is_invocable_v<const ReduceOp&, Type&&, Type&>;
  static constexpr bool is_rr_invocable =
      is_invocable_v<const ReduceOp&, Type&&, Type&&>;

  static_assert(
      is_cc_invocable
      || is_cl_invocable
      || is_cr_invocable
      || is_lc_invocable
      || is_ll_invocable
      || is_lr_invocable
      || is_rc_invocable
      || is_rl_invocable
      || is_rr_invocable,
      "Reducer must be invocable with value type.");

 public:
  constexpr do_merge_t(ReduceOp&& op)
  noexcept(std::is_nothrow_move_constructible_v<ReduceOp>)
  : op_(std::move(op))
  {}

  constexpr do_merge_t(const ReduceOp& op)
  noexcept(std::is_nothrow_copy_constructible_v<ReduceOp>)
  : op_(op)
  {}

  auto operator()(transport<value_type>& x, transport<value_type>&& y) const
  -> void {
    if constexpr(is_rr_invocable)
      this->assign_(x, std::invoke(op_, std::move(x).value(), std::move(y).value()));
    else if constexpr(is_lr_invocable || is_cr_invocable)
      this->assign_(x, std::invoke(op_, x.value(), std::move(y).value()));
    else if constexpr(is_rl_invocable || is_rc_invocable)
      this->assign_(x, std::invoke(op_, std::move(x).value(), y.value()));
    else
      this->assign_(x, std::invoke(op_, x.value(), y.value()));
  }

  auto operator()(transport<value_type>& x, transport<value_type&&>&& y) const
  -> void {
    if constexpr(is_rr_invocable)
      this->assign_(x, std::invoke(op_, std::move(x).value(), std::move(y).value()));
    else if constexpr(is_lr_invocable || is_cr_invocable)
      this->assign_(x, std::invoke(op_, x.value(), std::move(y).value()));
    else if constexpr(is_rl_invocable || is_rc_invocable)
      this->assign_(x, std::invoke(op_, std::move(x).value(), lref(y.value())));
    else
      this->assign_(x, std::invoke(op_, x.value(), lref(y.value())));
  }

  auto operator()(transport<value_type>& x, transport<const value_type&>&& y) const
  -> void {
    if constexpr(is_rc_invocable)
      this->assign_(x, std::invoke(op_, std::move(x).value(), y.value()));
    else if constexpr(is_lc_invocable || is_cc_invocable)
      this->assign_(x, std::invoke(op_, x.value(), y.value()));
    else if constexpr(is_rr_invocable)
      this->assign_(x, std::invoke(op_, std::move(x).value(), value_type(y.value())));
    else if constexpr(is_cr_invocable || is_lr_invocable)
      this->assign_(x, std::invoke(op_, x.value(), value_type(y.value())));
    else {
      value_type y_lvalue = y.value();
      if constexpr(is_rl_invocable)
        this->assign_(x, std::invoke(op_, std::move(x).value(), y_lvalue));
      else // is_ll_invocable || is_cl_invocable
        this->assign_(x, std::invoke(op_, x.value(), y_lvalue));
    }
  }

 private:
  template<typename Transport>
  static value_type& lref(value_type& x) noexcept {
    return x.value();
  }

  template<typename Transport>
  static value_type& lref(value_type&& x) noexcept {
    return x.value();
  }

  template<typename V>
  void assign_(transport<value_type>& x, V&& v) const {
    if (std::addressof(x.value()) != std::addressof(v))
      x.emplace(std::in_place_index<0>, std::forward<V>(v));
  }

  ReduceOp op_;
};


} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_MERGE_PIPE_H */
