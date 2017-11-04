#ifndef MONSOON_OBJPIPE_DETAIL_FILTER_OPERATION_H
#define MONSOON_OBJPIPE_DETAIL_FILTER_OPERATION_H

///\file
///\ingroup objpipe_detail

#include <monsoon/objpipe/detail/reader_intf.h>
#include <functional>

namespace monsoon {
namespace objpipe {
namespace detail {


/**
 * \brief A filter operation.
 * \ingroup objpipe_detail
 *
 * The reader interface will only emit objects matching the predicate.
 *
 * \tparam T The type of objects this operation filters on.
 * \tparam Pred An invokable predicate.
 */
template<typename T, typename Pred>
class filter_operation final
: public reader_intf<T>
{
 public:
  ///@copydoc reader_intf<T>::value_type
  using value_type = typename reader_intf<T>::value_type;
  ///@copydoc reader_intf<T>::pointer
  using pointer = typename reader_intf<T>::pointer;

  /**
   * Constructor.
   * @param[in] src The source from which to pull.
   * @param[in] args The arguments to create the predicate.
   */
  template<typename... Args>
  filter_operation(std::unique_ptr<reader_intf<T>, reader_release>&& src,
      Args&&... args)
  : pred_(std::forward<Args>(args)...),
    src_(std::move(src))
  {
    assert(src_ != nullptr);
  }

  ///@copydoc reader_intf<T>::is_pullable()
  auto is_pullable() const noexcept -> bool override {
    return offered_ != nullptr || src_->is_pullable();
  }

  ///@copydoc reader_intf<T>::wait()
  auto wait() const noexcept -> objpipe_errc override {
    if (offered_ != nullptr) return objpipe_errc::success;
    for (;;) {
      objpipe_errc e = objpipe_errc::success;
      if (test_predicate_(e) || e != objpipe_errc::success) return e;
      e = src_->pop_front();
      assert(e == objpipe_errc::success);
    }
  }

  ///@copydoc reader_intf<T>::empty()
  auto empty() const noexcept -> bool override {
    if (offered_ != nullptr) return false;
    for (;;) {
      if (src_->empty()) return true;
      objpipe_errc e;
      if (test_predicate_(e)) return false;
      assert(e == objpipe_errc::success);
      src_->pop_front();
    }
  }

  ///@copydoc reader_intf<T>::pull(objpipe_errc&)
  auto pull(objpipe_errc& e) -> std::optional<value_type> override {
    if (offered_ != nullptr) {
      e = objpipe_errc::success;
      value_type result = std::move(*std::exchange(offered_, nullptr));
      src_->pop_front();
      return result;
    }

    for (;;) {
      std::optional<value_type> result = src_->pull(e);
      if (!result.has_value() || test_predicate_(result.value()))
        return result;
    }
  }

  ///@copydoc reader_intf<T>::pull()
  auto pull() -> value_type override {
    if (offered_ != nullptr) {
      value_type result = std::move(*std::exchange(offered_, nullptr));
      src_->pop_front();
      return result;
    }

    for (;;) {
      value_type result = src_->pull();
      if (test_predicate_(result))
        return result;
    }
  }

  ///@copydoc reader_intf<T>::try_pull(objpipe_errc&)
  auto try_pull(objpipe_errc& e) -> std::optional<value_type> override {
    if (offered_ != nullptr) {
      e = objpipe_errc::success;
      value_type result = std::move(*std::exchange(offered_, nullptr));
      src_->pop_front();
      return result;
    }

    for (;;) {
      std::optional<value_type> result = src_->pull(e);
      if (!result.has_value() || test_predicate_(result.value()))
        return result;
    }
  }

  ///@copydoc reader_intf<T>::try_pull()
  auto try_pull() -> std::optional<value_type> override {
    if (offered_ != nullptr) {
      value_type result = std::move(*std::exchange(offered_, nullptr));
      src_->pop_front();
      return result;
    }

    for (;;) {
      std::optional<value_type> result = src_->try_pull();
      if (!result.has_value() || test_predicate_(result.value()))
        return result;
    }
  }

  ///@copydoc reader_intf<T>::front()
  auto front() const -> std::variant<pointer, objpipe_errc> override {
    using result_variant = std::variant<pointer, objpipe_errc>;

    while (offered_ == nullptr) {
      result_variant src_front = src_->front();
      if (src_front.index() == 1u)
        return src_front;
      else if (test_predicate_(*std::get<0>(src_front)))
        const_cast<filter_operation&>(*this).offered_ = std::get<0>(src_front);
      else {
        objpipe_errc e = src_->pop_front();
        if (e != objpipe_errc::success)
          return result_variant(std::in_place_index<1>, e);
      }
    }
    return result_variant(std::in_place_index<0>, offered_);
  }

  ///@copydoc reader_intf<T>::pop_front()
  auto pop_front() -> objpipe_errc override {
    if (offered_ == nullptr) {
      objpipe_errc e;
      while (!test_predicate_(e) && e == objpipe_errc::success) {
        e = src_->pop_front();
        if (e != objpipe_errc::success) break;
      }
      if (e != objpipe_errc::success) return e;
    }
    offered_ = nullptr;
    return src_->pop_front();
  }

  ///@copydoc reader_intf<T>::add_continuation(std::unique_ptr<continuation_intf,writer_release>&&)
  void add_continuation(std::unique_ptr<continuation_intf, writer_release>&& c)
      override {
    if (src_)
      src_->add_continuation(std::move(c));
  }

  ///@copydoc reader_intf<T>::erase_continuation(continuation_intf*)
  void erase_continuation(continuation_intf* c) override {
    if (src_)
      src_->erase_continuation(c);
  }

 private:
  ///copydoc base_objpipe::on_last_reader_gone_()
  void on_last_reader_gone_() noexcept override {
    src_.reset();
  }

  ///Never fires, as we don't attach.
  void on_last_writer_gone_() noexcept override {}

  bool test_predicate_(objpipe_errc& e) const {
    e = objpipe_errc::success;
    std::variant<pointer, objpipe_errc> v = src_->front();
    if (v.index() == 1u) e = std::get<1>(v);
    return v.index() == 0u && test_predicate_(*std::get<0>(v));
  }

  bool test_predicate_(std::add_lvalue_reference_t<std::add_const_t<value_type>> v)
      const {
    return std::invoke(pred_, v);
  }

  Pred pred_;
  std::unique_ptr<reader_intf<T>, reader_release> src_;
  pointer offered_ = nullptr;
};


}}} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_FILTER_OPERATION_H */
