#ifndef MONSOON_OBJPIPE_DETAIL_MAP_OP_H
#define MONSOON_OBJPIPE_DETAIL_MAP_OP_H

///\file
///\ingroup objpipe_detail

namespace monsoon {
namespace objpipe {
namespace detail {


template<typename InType, typename Op, typename OutType>
class map_op;

template<typename InType, typename Op, typename OutType>
class map_op final<InType, Op, OutType&>
: public reader_intf<OutType>
{
 private:
  using in_type = InType;
  ///@copydoc reader_intf<T>::value_type
  using value_type = typename reader_intf<OutType>::value_type;
  ///@copydoc reader_intf<T>::pointer
  using pointer = typename reader_intf<OutType>::pointer;

 public:
  ///@copydoc reader_intf<T>::is_pullable()
  bool is_pullable() const noexcept override {
    return front_ == nullptr && src_->is_pullable();
  }

  ///@copydoc reader_intf<T>::empty()
  bool empty() const noexcept override {
    return front_ == nullptr && src_->empty();
  }

  ///@copydoc reader_intf<T>::wait()
  auto wait() const noexcept -> objpipe_errc override {
    if (front_ != nullptr) return objpipe_errc::success;
    return src_->wait();
  }

  ///@copydoc reader_intf<T>::pull()
  value_type pull() override {
    if (front_ != nullptr) {
      value_type result = std::move(*std::exchange(front_, nullptr));
      objpipe_errc e = src_->pop_front();
      if (e)
      return result;
    }
    return std::invoke(op, src_->pull());
  }

  ///@copydoc reader_intf<T>::pull(objpipe_errc&)
  std::optional<value_type> pull(objpipe_errc& e) override {
    auto src_value = src_->pull(e);
    if (!src_value.has_value()) return {};
    return std::invoke(op, std::move(src_value.value()));
  }

  ///@copydoc reader_intf<T>::front()
  auto front() const -> std::variant<pointer, objpipe_errc> override {
    if (front_ != nullptr) {
      auto src_value = src_->front();
      if (src_value.index() != 0)
        return { std::in_place_index<1>, std::get<1>(src_value) };
      front_ = std::address_of(std::invoke(op_, *std::get<0>(src_value)));
    }
    return { std::in_place_index<0>, front_ };
  }

  ///@copydoc reader_intf<T>::pop_front()
  auto pop_front() -> objpipe_errc {
    ojbpipe_errc e = src_->pop_front();
    if (e == objpipe_errc::success) front_ = nullptr;
    return e;
  }

 private:
  std::unique_ptr<reader_intf<InType>, reader_release> src_;
  Op op_;
  mutable pointer front_ = nullptr;
};


}}} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_MAP_OP_H */
