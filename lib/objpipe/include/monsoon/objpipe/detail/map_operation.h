#ifndef MONSOON_OBJPIPE_DETAIL_MAP_OPERATION_H
#define MONSOON_OBJPIPE_DETAIL_MAP_OPERATION_H

///\file
///\ingroup objpipe_detail

namespace monsoon {
namespace objpipe {
namespace detail {


/**
 * Deduction of \ref map_operation result type.
 * \ingroup objpipe_detail
 */
#if __cplusplus >= 201703
template<typename InType, typename Op>
using map_out_type = std::invoke_result_t<
    std::add_lvalue_reference_t<std::add_const_t<Op>>,
    std::add_lvalue_reference_t<std::remove_reference_t<InType>>>;
#else
template<typename InType, typename Op>
using map_out_type = decltype(std::invoke(
    std::declval<std::add_lvalue_reference_t<std::add_const_t<Op>>>(),
    std::declval<std::add_lvalue_reference_t<std::remove_reference_t<InType>>>()));
#endif

/**
 * A transformation on objects in an objpipe.
 * \ingroup objpipe_detail
 */
template<typename InType, typename Op, typename OutType>
class map_operation final
: public reader_intf<std::decay_t<OutType>>
{
 private:
  ///\brief Element type of the objpipe that supplies values.
  using in_type = InType;
  ///@copydoc reader_intf<T>::value_type
  using value_type = typename reader_intf<OutType>::value_type;
  ///@copydoc reader_intf<T>::pointer
  using pointer = typename reader_intf<OutType>::pointer;

 public:
  /**
   * \brief Construct a map operation.
   * \tparam OpArgs Type of \p op_args, used to construct transform operation.
   * \param src The source objpipe, values of which are to be transformed.
   * \param op_args Arguments to construct the transform operation.
   */
  template<typename... OpArgs>
  explicit map_operation(
      std::unique_ptr<reader_intf<InType>, reader_release>&& src,
      OpArgs&&... op_args)
  : op_(std::forward<OpArgs>(op_args)...),
    src_(std::move(src))
  {
    if constexpr(std::is_reference_v<OutType>)
      front_ = nullptr;
  }

  ///@copydoc reader_intf<T>::is_pullable()
  bool is_pullable() const noexcept override {
    return front_ || src_->is_pullable();
  }

  ///@copydoc reader_intf<T>::empty()
  bool empty() const noexcept override {
    return !front_ && src_->empty();
  }

  ///@copydoc reader_intf<T>::wait()
  auto wait() const noexcept -> objpipe_errc override {
    if (front_) return objpipe_errc::success;
    return src_->wait();
  }

  ///@copydoc reader_intf<T>::pull()
  value_type pull() override {
    if (front_) {
      value_type result = std::move_if_noexcept(*front_);
      objpipe_errc e = src_->pop_front();
      if (e != objpipe_errc::success)
        throw std::system_error(static_cast<int>(e), objpipe_category());
      clear_front_();
      return result;
    }

    return map_apply_(src_->pull());
  }

  ///@copydoc reader_intf<T>::pull(objpipe_errc&)
  std::optional<value_type> pull(objpipe_errc& e) override {
    if (front_) {
      value_type result = std::move_if_noexcept(*front_);
      e = src_->pop_front();
      if (e != objpipe_errc::success)
        return {};
      clear_front_();
      return result;
    }

    auto src_value = src_->pull(e);
    if (!src_value.has_value()) return {};
    return map_apply_(std::move(src_value.value()));
  }

  ///@copydoc reader_intf<T>::try_pull(objpipe_errc&)
  std::optional<value_type> try_pull(objpipe_errc& e) override {
    if (front_) {
      value_type result = std::move_if_noexcept(*front_);
      e = src_->pop_front();
      if (e != objpipe_errc::success)
        return {};
      clear_front_();
      return result;
    }

    auto src_value = src_->pull(e);
    if (!src_value.has_value()) return {};
    return map_apply_(std::move(src_value.value()));
  }

  ///@copydoc reader_intf<T>::try_pull()
  std::optional<value_type> try_pull() override {
    if (front_) {
      value_type result = std::move_if_noexcept(*front_);
      objpipe_errc e = src_->pop_front();
      if (e != objpipe_errc::success)
        return {};
      clear_front_();
      return result;
    }

    auto src_value = src_->try_pull();
    if (!src_value.has_value()) return {};
    return map_apply_(std::move(src_value.value()));
  }

  ///@copydoc reader_intf<T>::front()
  auto front() const -> std::variant<pointer, objpipe_errc> override {
    objpipe_errc e = install_front_();
    if (e != objpipe_errc::success)
      return std::variant<pointer, objpipe_errc>(std::in_place_index<1>, e);
    return std::variant<pointer, objpipe_errc>(
	std::in_place_index<0>,
	std::addressof(*front_));
  }

  ///@copydoc reader_intf<T>::pop_front()
  auto pop_front() -> objpipe_errc override {
    objpipe_errc e = src_->pop_front();
    if (e == objpipe_errc::success) clear_front_();
    return e;
  }

  ///@copydoc reader_intf<T>::add_continuation(std::unique_ptr<continuation_intf,writer_release>&&)
  void add_continuation(std::unique_ptr<continuation_intf, writer_release>&& c) override {
    if (src_) src_->add_continuation(std::move(c));
  }

  ///@copydoc reader_intf<T>::erase_continuation(continuation_intf*)
  void erase_continuation(continuation_intf* c) override {
    if (src_) src_->erase_continuation(c);
  }

 private:
  void on_last_reader_gone_() noexcept override {
    src_.reset();
  }

  void on_last_writer_gone_() noexcept override {}

  auto map_apply_(
      std::add_lvalue_reference_t<std::remove_reference_t<InType>> in) const
  -> OutType {
    return std::invoke(op_, in);
  }

  auto map_apply_(
      std::add_rvalue_reference_t<std::remove_reference_t<InType>> in) const
  -> std::conditional_t<
      std::is_reference_v<OutType>,
      std::add_rvalue_reference_t<std::remove_reference_t<OutType>>,
      OutType> {
    return std::invoke(op_, in);
  }

  auto install_front_() const -> objpipe_errc {
    if (front_) return objpipe_errc::success;
    auto src_front = src_->front();
    if (src_front.index() == 1u) return std::get<1>(src_front);

    if constexpr(std::is_reference_v<OutType>)
      front_ = std::addressof(map_apply_(*std::get<0>(src_front)));
    else
      front_ = map_apply_(*std::get<0>(src_front));
    return objpipe_errc::success;
  }

  void clear_front_() {
    if constexpr(std::is_reference_v<OutType>)
      front_ = nullptr;
    else
      front_.reset();
  }

  mutable std::conditional_t<
          std::is_reference_v<OutType>,
          pointer,
          std::optional<value_type>>
      front_;
  Op op_;
  std::unique_ptr<reader_intf<InType>, reader_release> src_;
};


}}} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_MAP_OPERATION_H */
