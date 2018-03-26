#ifndef MONSOON_OBJPIPE_DETAIL_PUSH_OP_H
#define MONSOON_OBJPIPE_DETAIL_PUSH_OP_H

///\file
///\ingroup objpipe_detail
///\brief Operations for push based objpipe.

#include <exception>
#include <future>
#include <list>
#include <type_traits>
#include <utility>
#include <vector>
#include <monsoon/objpipe/push_policies.h>
#include <monsoon/objpipe/detail/adapt.h>
#include <monsoon/objpipe/detail/invocable_.h>
#include <monsoon/objpipe/detail/thread_pool.h>

namespace monsoon::objpipe::detail {


#if 0 // XXX example sink
template<typename Sink>
class push_adapter_t {
 public:
  constexpr push_adapter_t(Sink&& sink)
  : sink_(std::move(sink))
  {}

  // Required
  template<typename T>
  auto operator()(T&& v)
  noexcept(noexcept(std::declval<Sink&>()(std::declval<T>())))
  -> objpipe_errc {
    return sink_(v);
  }

  // Required
  template<typename T>
  auto push_exception(std::exception_ptr&& p)
  noexcept // May not throw
  -> void {
    sink_.push_exception(std::move(p));
  }

  ~push_adapter_t() noexcept {
#if 0
    try {
      sink_.close();
    } catch (...) {
      push_exception(std::current_exception());
    }
#endif
  }
};
#endif


namespace adapt_async {


template<typename ObjpipeVType, typename Init, typename Acceptor, typename Merger, typename Extractor>
class promise_reducer {
 public:
  using objpipe_value_type = ObjpipeVType;
  using state_type = std::remove_cv_t<std::remove_reference_t<decltype(std::declval<Init>()())>>;
  using value_type = std::remove_cv_t<std::remove_reference_t<decltype(std::declval<Extractor>()(std::declval<state_type>()))>>;

  template<typename InitArg, typename AcceptorArg, typename MergerArg, typename ExtractorArg>
  promise_reducer(std::promise<value_type>&& prom, InitArg&& init, AcceptorArg&& acceptor, MergerArg&& merger, ExtractorArg&& extractor)
  : prom_(std::move(prom)),
    init_(std::forward<InitArg>(init)),
    acceptor_(std::forward<AcceptorArg>(acceptor)),
    merger_(std::forward<MergerArg>(merger)),
    extractor_(std::forward<ExtractorArg>(extractor))
  {}

 private:
  ///\brief Shared state for unordered reductions.
  ///\ingroup objpipe_detail
  ///\details
  ///The shared state holds data common to all threads participating in
  ///a \ref multithread_unordered_push "unordered, multithreaded" reduction.
  class unordered_shared_state {
   public:
    using state_type = promise_reducer::state_type;
    using acceptor_type = Acceptor;

    unordered_shared_state(std::promise<value_type>&& prom, Init&& init, Acceptor&& acceptor, Merger&& merger, Extractor&& extractor)
    noexcept(std::is_nothrow_copy_constructible_v<std::promise<value_type>>
        && std::is_nothrow_copy_constructible_v<Acceptor>
        && std::is_nothrow_copy_constructible_v<Merger>
        && std::is_nothrow_copy_constructible_v<Extractor>)
    : prom_(std::move(prom)),
      init_(std::move(init)),
      acceptor_(std::move(acceptor)),
      merger_(std::move(merger)),
      extractor_(std::move(extractor))
    {}

    unordered_shared_state(unordered_shared_state&&) = delete;
    unordered_shared_state(const unordered_shared_state&) = delete;
    unordered_shared_state& operator=(unordered_shared_state&&) = delete;
    unordered_shared_state& operator=(const unordered_shared_state&) = delete;

    ~unordered_shared_state() noexcept {
      if (!bad_.load(std::memory_order_acquire)) {
        try {
          if (pstate_.has_value()) { // Should only fail to hold, if constructors fail.
            if constexpr(std::is_same_v<void, value_type>) {
              std::invoke(std::move(extractor_), std::move(*pstate_));
              prom_.set_value();
            } else {
              prom_.set_value(std::invoke(std::move(extractor_), std::move(*pstate_)));
            }
          }
        } catch (...) {
          push_exception(std::current_exception());
        }
      }
    }

    bool is_bad() const noexcept {
      return bad_.load(std::memory_order_relaxed);
    }

    auto push_exception(std::exception_ptr exptr)
    noexcept
    -> void {
      bool expect_bad = false;
      if (bad_.compare_exchange_strong(expect_bad, true, std::memory_order_acq_rel, std::memory_order_acquire)) {
        try {
          prom_.set_exception(std::move(exptr));
        } catch (const std::future_error& e) {
          if (e.code() == std::future_errc::promise_already_satisfied) {
            /* SKIP (swallow exception) */
          } else { // Only happens if the promise is in a bad state.
            throw; // aborts, because noexcept, this is intentional
          }
        }
      }
    }

    auto publish(state_type&& state)
    noexcept
    -> void {
      if (is_bad()) return; // Discard if objpipe went bad.

      try {
        std::unique_lock<std::mutex> lck{ mtx_ };

        for (;;) {
          if (!pstate_.has_value()) {
            pstate_.emplace(std::move(state));
            return;
          }

          state_type merge_into_ = std::move(pstate_).value();
          pstate_.reset();

          // Release lock during merge operation.
          lck.unlock();
          std::invoke(merger(), state, std::move(merge_into_));
          if (is_bad()) return; // Discard if objpipe went bad.
          lck.lock();
        }
      } catch (...) {
        push_exception(std::current_exception());
      }
    }

    auto new_state() const
    noexcept(is_nothrow_invocable_v<const Init&>)
    -> state_type {
      return std::invoke(init());
    }

    auto new_state([[maybe_unused]] const state_type& s) const
    noexcept(is_nothrow_invocable_v<const Init&>)
    -> state_type {
      return new_state();
    }

    auto init() const
    noexcept
    -> const Init& {
      return init_;
    }

    auto acceptor() const
    noexcept
    -> const acceptor_type& {
      return acceptor_;
    }

    auto merger() const
    noexcept
    -> const Merger& {
      return merger_;
    }

   private:
    std::atomic<bool> bad_{ false };
    std::mutex mtx_;
    std::optional<state_type> pstate_;
    std::promise<value_type> prom_;
    Init init_;
    acceptor_type acceptor_;
    Merger merger_;
    Extractor extractor_;
  };

  ///\brief Shared state for ordered reductions.
  ///\ingroup objpipe_detail
  ///\details
  ///The shared state holds data common to all threads participating in
  ///a \ref multithread_push "ordered, multithreaded" reduction.
  class ordered_shared_state {
   private:
    class internal_state {
     public:
      internal_state(promise_reducer::state_type&& state)
      noexcept(std::is_nothrow_move_constructible_v<promise_reducer::state_type>)
      : state(std::move(state))
      {}

      internal_state(const internal_state&) = delete;
      internal_state(internal_state&&) = delete;
      internal_state& operator=(const internal_state&) = delete;
      internal_state& operator=(internal_state&&) = delete;

      promise_reducer::state_type state;
      bool ready = false;
    };

    using internal_state_list = std::list<internal_state>;

   public:
    using state_type = typename internal_state_list::iterator;

    class acceptor_type {
     public:
      acceptor_type(Acceptor&& impl)
      noexcept(std::is_nothrow_move_constructible_v<Acceptor>)
      : impl_(std::move(impl))
      {}

      template<typename Arg>
      auto operator()(state_type& s, Arg&& v) const
      noexcept(is_nothrow_invocable_v<Acceptor, state_type&, Arg>)
      -> void {
        std::invoke(impl_, s.state, std::forward<Arg>(v));
      }

     private:
      Acceptor impl_;
    };

    ordered_shared_state(std::promise<value_type>&& prom, Init&& init, Acceptor&& acceptor, Merger&& merger, Extractor&& extractor)
    noexcept(std::is_nothrow_copy_constructible_v<std::promise<value_type>>
        && std::is_nothrow_copy_constructible_v<Acceptor>
        && std::is_nothrow_copy_constructible_v<Merger>
        && std::is_nothrow_copy_constructible_v<Extractor>)
    : prom_(std::move(prom)),
      init_(std::move(init)),
      acceptor_(std::move(acceptor)),
      merger_(std::move(merger)),
      extractor_(std::move(extractor))
    {}

    ordered_shared_state(ordered_shared_state&&) = delete;
    ordered_shared_state(const ordered_shared_state&) = delete;
    ordered_shared_state& operator=(ordered_shared_state&&) = delete;
    ordered_shared_state& operator=(const ordered_shared_state&) = delete;

    ~ordered_shared_state() noexcept {
      if (!bad_.load(std::memory_order_acquire)) {
        try {
          if (pstate_.size() == 1u && pstate_.front().ready) { // Should only fail to hold, if constructors fail.
            if constexpr(std::is_same_v<void, value_type>) {
              std::invoke(std::move(extractor_), std::move(pstate_.front().state));
              prom_.set_value();
            } else {
              prom_.set_value(std::invoke(std::move(extractor_), std::move(pstate_.front().state)));
            }
          }
        } catch (...) {
          push_exception(std::current_exception());
        }
      }
    }

    bool is_bad() const noexcept {
      return bad_.load(std::memory_order_relaxed);
    }

    auto push_exception(std::exception_ptr exptr)
    noexcept
    -> void {
      bool expect_bad = false;
      if (bad_.compare_exchange_strong(expect_bad, true, std::memory_order_acq_rel, std::memory_order_acquire)) {
        try {
          prom_.set_exception(std::move(exptr));
        } catch (const std::future_error& e) {
          if (e.code() == std::future_errc::promise_already_satisfied) {
            /* SKIP (swallow exception) */
          } else { // Only happens if the promise is in a bad state.
            throw; // aborts, because noexcept, this is intentional
          }
        }
      }
    }

    auto publish(state_type state)
    noexcept
    -> void {
      if (is_bad()) return; // Discard if objpipe went bad.

      try {
        std::unique_lock<std::mutex> lck{ mtx_ };

        for (;;) { // Note: state is an iterator in pstate_.
          assert(!state->ready);

          if (state != pstate_.begin() && std::prev(state)->ready) {
            state_type predecessor = std::prev(state);
            predecessor->ready = false;

            // Perform merge with lock released.
            lck.unlock();
            std::invoke(merger(), predecessor->state, std::move(state->state));
            if (is_bad()) return; // Discard if objpipe went bad.
            lck.lock();

            // Erase state, as it is merged into predecessor.
            // Continue by trying to further merge predecessor.
            pstate_.erase(state);
            state = predecessor;
          } else if (std::next(state) != pstate_.end() && std::next(state)->ready) {
            state_type successor = std::next(state);
            successor->ready = false;

            // Perform merge with lock released.
            lck.unlock();
            std::invoke(merger(), state->state, std::move(successor->state));
            if (is_bad()) return; // Discard if objpipe went bad.
            lck.lock();

            // Erase successor, as it is merged into state.
            // Continue by trying to further merge state.
            pstate_.erase(successor);
          } else {
            state->ready = true;
            return;
          }
        }
      } catch (...) {
        push_exception(std::current_exception());
      }
    }

    auto new_state()
    -> state_type {
      std::lock_guard<std::mutex> lck{ mtx_ };

      return pstate_.emplace(pstate_.end(), std::invoke(init()));
    }

    auto new_state(const state_type& pos)
    -> state_type {
      std::lock_guard<std::mutex> lck{ mtx_ };

      return pstate_.emplace(std::next(pos), std::invoke(init()));
    }

    auto init() const
    noexcept
    -> const Init& {
      return init_;
    }

    auto acceptor() const
    noexcept
    -> const acceptor_type& {
      return acceptor_;
    }

    auto merger() const
    noexcept
    -> const Merger& {
      return merger_;
    }

   private:
    std::atomic<bool> bad_{ false };
    std::mutex mtx_;
    internal_state_list pstate_;
    std::promise<value_type> prom_;
    Init init_;
    acceptor_type acceptor_;
    Merger merger_;
    Extractor extractor_;
  };

  ///\brief Reducer state for multithread push.
  ///\ingroup objpipe_detail
  ///\details
  ///This reducer state has logic to allow multiple copies to cooperate
  ///in a reduction.
  template<typename SharedState>
  class local_state {
   public:
    using state_type = typename SharedState::state_type;
    using acceptor_type = typename SharedState::acceptor_type;

   private:
    ///\brief Initializing constructor.
    ///\details Initializes the local state using \p sptr.
    ///\param[in] sptr A newly constructed shared state.
    explicit local_state(std::shared_ptr<SharedState> sptr)
    noexcept(std::is_nothrow_constructible_v<state_type, decltype(std::declval<SharedState&>().new_state())>
        && noexcept(std::declval<SharedState&>().new_state())
        && std::is_nothrow_move_constructible_v<state_type>
        && is_nothrow_invocable_v<Init>)
    : state_(sptr->new_state()),
      acceptor_(sptr->acceptor()),
      sptr_(std::move(sptr))
    {}

    ///\brief Sibling constructor.
    ///\details Used to construct a sibling state.
    ///The sibling state is positioned after \p s.
    explicit local_state(std::shared_ptr<SharedState> sptr, const state_type& s)
    noexcept(std::is_nothrow_constructible_v<state_type, decltype(std::declval<SharedState&>().new_state(std::declval<const state_type&>()))>
        && noexcept(std::declval<SharedState&>().new_state(std::declval<const state_type&>()))
        && std::is_nothrow_move_constructible_v<state_type>
        && is_nothrow_invocable_v<Init>)
    : state_(sptr->new_state(s)),
      acceptor_(sptr->acceptor()),
      sptr_(std::move(sptr))
    {}

   public:
    ///\brief Construct a new local state.
    ///\param[in] prom The promise to fulfill at the end of the reduction.
    ///\param[in] init A functor constructing an initial reduction-state.
    ///\param[in] acceptor A functor combining values into the reduction state.
    ///\param[in] merger A functor merging the right-hand-side reduction-state into the left-hand-side reduction-state.
    ///\param[in] extractor A functor to retrieve the reduce outcome from the reduction-state.
    local_state(std::promise<value_type> prom,
        Init&& init,
        Acceptor&& acceptor,
        Merger&& merger,
        Extractor&& extractor)
    : local_state(std::make_shared<SharedState>(
            std::move(prom),
            std::move(init),
            std::move(acceptor),
            std::move(merger),
            std::move(extractor)))
    {}

    ///\brief Copy constructor creates a sibling state.
    ///\details Sibling state is positioned directly after rhs.
    ///\note If the shared state is unordered, positional information is not maintained.
    local_state(const local_state& rhs)
    : local_state(rhs.sptr_, rhs.state_)
    {}

    ///\brief Move constructor.
    local_state(local_state&& rhs)
    noexcept(std::is_nothrow_move_constructible_v<state_type>)
    = default;

    ///\brief Acceptor for rvalue reference.
    ///\details Accepts a single value by rvalue reference.
    ///\param[in] v The accepted value.
    auto operator()(objpipe_value_type&& v)
    -> objpipe_errc {
      if (sptr_->is_bad()) return objpipe_errc::bad;

      if constexpr(is_invocable_v<acceptor_type, state_type&, objpipe_value_type&&>)
        return std::invoke(acceptor_, state_, std::move(v));
      else
        return std::invoke(acceptor_, state_, v);
    }

    ///\brief Acceptor for const reference.
    ///\details Accepts a single value by const reference.
    ///\param[in] v The accepted value.
    auto operator()(const objpipe_value_type& v)
    -> objpipe_errc {
      if (sptr_->is_bad()) return objpipe_errc::bad;

      if constexpr(is_invocable_v<acceptor_type, state_type&, const objpipe_value_type&>)
        return std::invoke(acceptor_, state_, v);
      else
        return (*this)(value_type(v));
    }

    ///\brief Acceptor for lvalue reference.
    ///\details Accepts a single value by lvalue reference.
    ///\param[in] v The accepted value.
    auto operator()(objpipe_value_type& v)
    -> objpipe_errc {
      if (sptr_->is_bad()) return objpipe_errc::bad;

      if constexpr(is_invocable_v<acceptor_type, state_type&, objpipe_value_type&>)
        return std::invoke(acceptor_, state_, v);
      else
        return (*this)(value_type(v));
    }

    ///\brief Acceptor for exceptions.
    ///\details Immediately completes the promise with the exception.
    ///\param[in] exptr An exception pointer.
    auto push_exception(std::exception_ptr exptr)
    noexcept
    -> void {
      sptr_->push_exception(std::move(exptr));
    }

    ///\brief Destructor, publishes the reduction-state.
    ~local_state() noexcept {
      if (sptr_ != nullptr) // May have been moved.
        sptr_->publish(std::move(state_));
    }

   private:
    ///\brief Reduction-state.
    ///\details Values are accepted into the reduction state.
    state_type state_;
    ///\brief Value acceptor functor.
    ///\note Life time is bounded by sptr_.
    const acceptor_type& acceptor_;
    ///\brief Shared state pointer.
    std::shared_ptr<SharedState> sptr_;
  };

  ///\brief Single thread reducer state.
  ///\details
  ///Accepts values and fills in the associated promise when destroyed.
  ///\note Single thread state is not copy constructible.
  class single_thread_state {
   public:
    single_thread_state(std::promise<value_type>&& prom, Init&& init, Acceptor&& acceptor, Extractor&& extractor)
    noexcept(std::is_nothrow_constructible_v<state_type, invoke_result_t<Init>>
        && std::is_nothrow_move_constructible_v<std::promise<value_type>>
        && std::is_nothrow_move_constructible_v<Acceptor>
        && std::is_nothrow_move_constructible_v<Extractor>
        && is_nothrow_invocable_v<Init>)
    : prom_(std::move(prom)),
      state_(std::invoke(std::move(init))),
      acceptor_(std::move(acceptor)),
      extractor_(std::move(extractor))
    {}

    single_thread_state(single_thread_state&& other)
    noexcept(std::is_nothrow_move_constructible_v<Acceptor>
        && std::is_nothrow_move_constructible_v<Extractor>
        && std::is_nothrow_move_constructible_v<state_type>)
    : prom_(std::move(other.prom_)),
      bad_(std::exchange(other.bad_, true)), // Ensure other won't attempt to assign a value at destruction.
      state_(std::move(other.state_)),
      acceptor_(std::move(other.acceptor_)),
      extractor_(std::move(other.extractor_))
    {}

    single_thread_state(const single_thread_state&) = delete;
    single_thread_state& operator=(single_thread_state&&) = delete;
    single_thread_state& operator=(const single_thread_state&) = delete;

    ///\brief Accept a value by rvalue reference.
    auto operator()(objpipe_value_type&& v)
    -> objpipe_errc {
      if (bad_) return objpipe_errc::bad;

      if constexpr(is_invocable_v<Acceptor, state_type&, objpipe_value_type&&>)
        return std::invoke(acceptor_, state_, std::move(v));
      else
        return std::invoke(acceptor_, state_, v);
    }

    ///\brief Accept a value by const reference.
    auto operator()(const objpipe_value_type& v)
    -> objpipe_errc {
      if (bad_) return objpipe_errc::bad;

      if constexpr(is_invocable_v<Acceptor, state_type&, const objpipe_value_type&>)
        return std::invoke(acceptor_, state_, v);
      else
        return (*this)(value_type(v));
    }

    ///\brief Accept a value by lvalue reference.
    auto operator()(objpipe_value_type& v)
    -> objpipe_errc {
      if (bad_) return objpipe_errc::bad;

      if constexpr(is_invocable_v<Acceptor, state_type&, objpipe_value_type&>)
        return std::invoke(acceptor_, state_, v);
      else
        return (*this)(value_type(v));
    }

    ///\brief Accept an exception.
    ///\details
    ///Immediately completes the promise using the exception pointer.
    auto push_exception(std::exception_ptr exptr)
    noexcept
    -> void {
      try {
        if (!bad_)
          prom_.set_exception(std::move(exptr));
        bad_ = true;
      } catch (const std::future_error& e) {
        if (e.code() == std::future_errc::promise_already_satisfied) {
          /* SKIP (swallow exception) */
        } else { // Only happens if the promise is in a bad state.
          throw; // aborts, because noexcept, this is intentional
        }
      }
    }

    ///\brief Destructor, publishes the result of the reduction.
    ///\details
    ///Unless the promise is already completed, the reduction outcome
    ///is assigned.
    ~single_thread_state() noexcept {
      if (!bad_) {
        try {
          if constexpr(std::is_same_v<void, value_type>) {
            std::invoke(std::move(extractor_), std::move(state_));
            prom_.set_value();
          } else {
            prom_.set_value(std::invoke(std::move(extractor_), std::move(state_)));
          }
        } catch (...) {
          push_exception(std::current_exception());
        }
      }
    }

   private:
    ///\brief The promise to fulfill at the end of the reduction.
    std::promise<value_type> prom_;
    ///\brief Indicator that gets set if an exception is pushed.
    ///\details Used to quickly abort the reduction if an exception is published.
    bool bad_ = false;
    ///\brief Reduction state.
    ///\details Only a single instance of the reduction state is used.
    state_type state_;
    ///\brief Acceptor functor.
    ///\details Accepts values into state_.
    Acceptor acceptor_;
    ///\brief Extractor functor.
    ///\details Used to retrieve the result of the reduction.
    Extractor extractor_;
  };

 public:
  ///\brief Create a new reducer for
  ///\ref singlethread_push "single threaded push" and
  ///\ref existingthread_push "existing-only single thread push".
  auto new_state(existingthread_push tag)
  -> single_thread_state {
    // Merger is not forwarded, since single threaded reduction does not perform merging of reducer states.
    return single_thread_state(
        std::move(prom_),
        std::move(init_),
        std::move(acceptor_),
        std::move(extractor_));
  }

  ///\brief Create a new reducer for \ref multithread_push "ordered, multi threaded push".
  auto new_state(multithread_push tag)
  -> local_state<ordered_shared_state> {
    return local_state<ordered_shared_state>(
        std::move(prom_),
        std::move(init_),
        std::move(acceptor_),
        std::move(merger_),
        std::move(extractor_));
  }

  ///\brief Create a new reducer for \ref multithread_unordered_push "unordered, multi threaded push".
  auto new_state(multithread_unordered_push tag)
  -> local_state<unordered_shared_state> {
    return local_state<unordered_shared_state>(
        std::move(prom_),
        std::move(init_),
        std::move(acceptor_),
        std::move(merger_),
        std::move(extractor_));
  }

 private:
  ///\brief Promise to complete at the end of the reduction.
  std::promise<value_type> prom_;
  ///\brief Initial state functor, creates a reducer-state.
  Init init_;
  ///\brief Accept functor, accepts values into the reducer-state.
  Acceptor acceptor_;
  ///\brief Merge functor, merges two reducer-states.
  ///\details
  ///The completion of the merge must be placed into the left argument.
  ///
  ///In other words:
  ///\code
  ///void Merger(ReducerState& x, ReducerState&& y) {
  ///  ...
  ///}
  ///\endcode
  ///In this example, the values of \p y must be merged into the values of \p x.
  Merger merger_;
  ///\brief Extract functor, retrieves the reduce outcome from the reducer-state.
  Extractor extractor_;
};


template<typename Fn, typename... Args>
class task {
 public:
  template<typename FnArg, typename... ArgsArg, typename = std::enable_if_t<sizeof...(ArgsArg) == sizeof...(Args)>>
  explicit task(FnArg&& fn, ArgsArg&&... args)
  noexcept(std::conjunction_v<
      std::is_nothrow_constructible<Fn, FnArg>,
      std::is_nothrow_constructible<Args, ArgsArg>...>)
  : fn_(std::forward<FnArg>(fn)),
    args_(std::forward<ArgsArg>(args)...)
  {}

  task(task&& rhs)
  noexcept(std::conjunction_v<
      std::is_nothrow_move_constructible<Fn>,
      std::is_nothrow_move_constructible<Args>...>)
  : fn_(std::move(rhs.fn_)),
    args_(std::move(rhs.args_))
  {}

  auto operator()()
  noexcept(noexcept(std::apply(std::declval<Fn>(), std::declval<std::tuple<Args...>>())))
  -> auto {
    return std::apply(std::move(fn_), std::move(args_));
  }

 private:
  Fn fn_;
  std::tuple<Args...> args_;
};

template<typename Fn, typename... Args>
auto make_task(Fn&& fn, Args&&... args)
-> task<std::decay_t<Fn>, std::remove_cv_t<std::remove_reference_t<Args>>...> {
  return task<std::decay_t<Fn>, std::remove_cv_t<std::remove_reference_t<Args>>...>(
      std::forward<Fn>(fn),
      std::forward<Args>(args)...);
}


} /* namespace monsoon::objpipe::detail::adapt_async */


template<typename Acceptor>
struct acceptor_adapter {
 public:
  explicit acceptor_adapter(Acceptor&& acceptor)
  noexcept(std::is_nothrow_move_constructible_v<Acceptor>)
  : acceptor_(std::move(acceptor))
  {}

  explicit acceptor_adapter(const Acceptor& acceptor)
  noexcept(std::is_nothrow_copy_constructible_v<Acceptor>)
  : acceptor_(acceptor)
  {}

  template<typename State, typename T>
  auto operator()(State& s, T&& v) const
  -> std::enable_if_t<std::is_rvalue_reference_v<T&&>, objpipe_errc> {
    if constexpr(is_invocable_v<Acceptor, State&, T&&>)
      return std::invoke(acceptor_, s, std::move(v));
    else
      return std::invoke(acceptor_, s, v);
  }

  template<typename State, typename T>
  auto operator()(State& s, T& v) const
  -> std::enable_if_t<std::is_lvalue_reference_v<T&>, objpipe_errc> {
    if constexpr(is_invocable_v<Acceptor, State&, T&>)
      return std::invoke(acceptor_, s, v);
    else
      return (*this)(s, T(v));
  }

 private:
  Acceptor acceptor_;
};


namespace adapt {
namespace {


struct mock_push_adapter_ {
  template<typename T>
  auto operator()(T&& v) -> void;

  template<typename T>
  auto push_exception(std::exception_ptr) -> void;
};

template<typename Source, typename PushTag, typename = void>
struct has_ioc_push_
: std::false_type
{};

template<typename Source, typename PushTag>
struct has_ioc_push_<Source, PushTag, std::void_t<decltype(std::declval<Source>().ioc_push(std::declval<PushTag&>(), std::declval<mock_push_adapter_>()))>>
: std::true_type
{};

} /* namespace monsoon::objpipe::detail::adapt::<unnamed> */


///\brief Test if source has an ioc_push method for the given push tag.
template<typename Source, typename PushTag>
constexpr bool has_ioc_push = has_ioc_push_<Source, PushTag>::value;


template<typename Source, typename Acceptor>
auto ioc_push(Source&& src, existingthread_push push_tag, Acceptor&& acceptor)
-> std::enable_if_t<has_ioc_push<std::decay_t<Source>, existingthread_push>> {
  static_assert(std::is_rvalue_reference_v<Source&&>
      && !std::is_const_v<Source&&>,
      "Source must be passed by (non-const) rvalue reference.");
  return std::forward<Source>(src).ioc_push(
      std::move(push_tag),
      std::forward<Acceptor>(acceptor));
}

template<typename Source, typename Acceptor>
auto ioc_push(Source&& src, singlethread_push push_tag, Acceptor&& acceptor)
-> std::enable_if_t<has_ioc_push<std::decay_t<Source>, singlethread_push>> {
  static_assert(std::is_rvalue_reference_v<Source&&>
      && !std::is_const_v<Source&&>,
      "Source must be passed by (non-const) rvalue reference.");
  return std::forward<Source>(src).ioc_push(
      std::move(push_tag),
      std::forward<Acceptor>(acceptor));
}

template<typename Source, typename Acceptor>
auto ioc_push(Source&& src, multithread_push push_tag, Acceptor&& acceptor)
-> std::enable_if_t<has_ioc_push<std::decay_t<Source>, multithread_push>> {
  static_assert(std::is_rvalue_reference_v<Source&&>
      && !std::is_const_v<Source&&>,
      "Source must be passed by (non-const) rvalue reference.");
  return std::forward<Source>(src).ioc_push(
      std::move(push_tag),
      std::forward<Acceptor>(acceptor));
}

template<typename Source, typename Acceptor>
auto ioc_push(Source&& src, multithread_unordered_push push_tag, Acceptor&& acceptor)
-> std::enable_if_t<has_ioc_push<std::decay_t<Source>, multithread_unordered_push>> {
  static_assert(std::is_rvalue_reference_v<Source&&>
      && !std::is_const_v<Source&&>,
      "Source must be passed by (non-const) rvalue reference.");
  return std::forward<Source>(src).ioc_push(
      std::move(push_tag),
      std::forward<Acceptor>(acceptor));
}

template<typename Source, typename Acceptor>
auto ioc_push(Source&& src, [[maybe_unused]] singlethread_push push_tag, Acceptor&& acceptor)
-> std::enable_if_t<!has_ioc_push<std::decay_t<Source>, singlethread_push>> {
  static_assert(std::is_rvalue_reference_v<Source&&>
      && !std::is_const_v<Source&&>,
      "Source must be passed by (non-const) rvalue reference.");
  thread_pool::default_pool()
      .publish(
          adapt_async::make_task(
              [](Source&& src, std::decay_t<Acceptor>&& acceptor) {
                try {
                  for (;;) {
                    auto tx = adapt::raw_pull(src);
                    using value_type = typename decltype(tx)::value_type;

                    if (tx.errc() == objpipe_errc::closed) {
                      break;
                    } else if (tx.errc() != objpipe_errc::success) {
                      throw objpipe_error(tx.errc());
                    } else {
                      assert(tx.has_value());
                      objpipe_errc e;
                      if constexpr(is_invocable_v<std::decay_t<Acceptor>&, typename decltype(tx)::type>)
                        e = std::invoke(acceptor, std::move(tx).value());
                      else if constexpr(is_invocable_v<std::decay_t<Acceptor>&, value_type> && std::is_const_v<typename decltype(tx)::type>)
                        e = std::invoke(acceptor, std::move(tx).by_value().value());
                      else
                        e = std::invoke(acceptor, tx.ref());

                      if (e != objpipe_errc::success)
                        break;
                    }
                  }
                } catch (...) {
                  acceptor.push_exception(std::current_exception());
                }
              },
              std::move(src),
              std::forward<Acceptor>(acceptor))
      );
}

template<typename Source, typename Acceptor>
auto ioc_push(Source&& src, [[maybe_unused]] existingthread_push push_tag, Acceptor&& acceptor)
-> std::enable_if_t<!has_ioc_push<std::decay_t<Source>, singlethread_push>> {
  static_assert(std::is_rvalue_reference_v<Source&&>
      && !std::is_const_v<Source&&>,
      "Source must be passed by (non-const) rvalue reference.");
  throw objpipe_error(objpipe_errc::no_thread);
}


} /* namespace monsoon::objpipe::detail::adapt */


struct noop_merger {
  template<typename T>
  void operator()(T& x, T&& y) const {}
};

struct void_extractor {
  template<typename T>
  void operator()(T&& v) const {}
};


template<typename Source, typename PushTag>
class async_adapter_t {
 public:
  using value_type = adapt::value_type<Source>;

  explicit async_adapter_t(Source&& src, PushTag push_tag = PushTag())
  noexcept(std::is_nothrow_move_constructible_v<Source>
      && std::is_nothrow_move_constructible_v<PushTag>)
  : src_(std::move(src)),
    push_tag_(std::move(push_tag))
  {}

  ///\brief Reduce operation.
  ///\note \p init is a functor.
  template<typename Init, typename Acceptor, typename Merger, typename Extractor>
  auto reduce(Init&& init, Acceptor&& acceptor, [[maybe_unused]] Merger&& merger, Extractor&& extractor)
  -> std::future<std::remove_cv_t<std::remove_reference_t<decltype(std::declval<Extractor&>()(std::declval<Init>()()))>>> {
    static_assert(is_invocable_v<const std::decay_t<Init>&>,
        "Init must be invocable, returning a reducer state.");

    using state_type = std::remove_cv_t<std::remove_reference_t<invoke_result_t<Init>>>;

    static_assert(is_invocable_v<std::decay_t<const Acceptor&>, state_type&, value_type&&>
        || is_invocable_v<std::decay_t<Acceptor>, state_type&, const value_type&>
        || is_invocable_v<std::decay_t<Acceptor>, state_type&, value_type&>,
        "Acceptor must be invocable with reducer-state reference and value_type (rref/lref/cref).");
    static_assert(is_invocable_v<const std::decay_t<Merger>&, state_type&, state_type&&>,
        "Merger must be invocable with lref reducer-state and rref reducer-state.");
    static_assert(is_invocable_v<std::decay_t<Extractor>&&, state_type&&>,
        "Extractor must be invocable with rref reducer-state.");

    using result_type = std::remove_cv_t<std::remove_reference_t<decltype(std::declval<Extractor&>()(std::declval<state_type>()))>>;

    // Default case: use ioc_push to perform push operation.
    // Note the double if:
    // - one to test if the code is valid,
    // - one to test if the function is likely to succeed.
    if constexpr(adapt::has_ioc_push<Source, PushTag>) {
      if (src_.can_push(push_tag_)) {
        std::promise<result_type> p;
        std::future<result_type> f = p.get_future();

        adapt::ioc_push(
            std::move(src_),
            std::move(push_tag_),
            adapt_async::promise_reducer<value_type, std::decay_t<Init>, std::decay_t<Acceptor>, std::decay_t<Merger>, std::decay_t<Extractor>>(std::move(p), std::forward<Init>(init), std::forward<Acceptor>(acceptor), std::forward<Merger>(merger), std::forward<Extractor>(extractor)).new_state(push_tag_));

        return f;
      }
    }

    // Alternative case: use (lazy or threaded) future to perform reduction.
    return std::async(
        std::is_same_v<existingthread_push, PushTag> ? std::launch::deferred : std::launch::async,
        [](Source&& src, std::decay_t<Init>&& init, const acceptor_adapter<std::decay_t<Acceptor>>& acceptor, std::decay_t<Extractor>&& extractor) -> result_type {
          auto state = std::invoke(std::move(init));
          for (;;) {
            auto tx_val = adapt::raw_pull(src);
            if (tx_val.has_value()) {
              std::invoke(acceptor, state, std::move(tx_val).value());
            } else if (tx_val.errc() == objpipe_errc::closed) {
              break;
            } else {
              throw objpipe_error(tx_val.errc());
            }
          }

          return std::invoke(std::move(extractor), std::move(state));
        },
        std::move(src_),
        std::forward<Init>(init),
        acceptor_adapter<std::decay_t<Acceptor>>(std::forward<Acceptor>(acceptor)),
        std::forward<Extractor>(extractor));
  }

  ///\brief Reduce operation.
  ///\note \p init is a value.
  template<typename Init, typename Acceptor>
  auto reduce(Init&& init, Acceptor&& acceptor)
  -> std::future<std::remove_cv_t<std::remove_reference_t<Init>>> {
    using result_type = std::remove_cv_t<std::remove_reference_t<Init>>;

    return reduce(
        [init] { return init; },
        [acceptor](auto&& x, auto&& y) {
          std::invoke(acceptor, x, y);
          return objpipe_errc::success;
        },
        acceptor,
        [](result_type&& v) -> result_type&& { return std::move(v); });
  }

  template<typename Alloc = std::allocator<adapt::value_type<Source>>>
  auto to_vector(Alloc alloc = Alloc()) &&
  -> std::future<std::vector<value_type, Alloc>> {
    using result_type = std::vector<value_type, Alloc>;

    return reduce(
        [alloc]() { return result_type(alloc); },
        [](result_type& vector, auto&& v) {
          vector.push_back(std::forward<decltype(v)>(v));
          return objpipe_errc::success;
        },
        [](result_type& vector, result_type&& rhs) {
          vector.insert(vector.end(), std::make_move_iterator(rhs.begin()), std::make_move_iterator(rhs.end()));
        },
        [](result_type&& vector) -> result_type&& {
          return std::move(vector);
        });
  }

  template<typename OutputIterator>
  auto copy(OutputIterator&& out) &&
  -> std::future<void> {
    return reduce(
        [out]() { return out; },
        [](std::decay_t<OutputIterator>& out, auto&& v) {
          *out++ = std::forward<decltype(v)>(v);
          return objpipe_errc::success;
        },
        noop_merger(),
        void_extractor());
  }

  template<typename Fn>
  auto for_each(Fn&& fn) &&
  -> std::future<void> {
    return reduce(
        [fn]() { return fn; },
        [](std::decay_t<Fn>& fn, auto&& v) {
          fn(std::forward<decltype(v)>(v));
          return objpipe_errc::success;
        },
        noop_merger(),
        void_extractor());
  }

  auto count() &&
  -> std::future<std::uintptr_t> {
    return reduce(
        []() -> std::uintptr_t { return 0u; },
        [](std::uintptr_t& c, [[maybe_unused]] const auto& v) {
          ++c;
          return objpipe_errc::success;
        },
        [](std::uintptr_t& x, std::uintptr_t&& y) { x += y; },
        [](std::uintptr_t&& c) -> std::uintptr_t { return c; });
  }

 private:
  Source src_;
  PushTag push_tag_;
};


} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_PUSH_OP_H */
