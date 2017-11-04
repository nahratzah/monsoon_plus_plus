#ifndef MONSOON_OBJPIPE_DETAIL_ARRAYED_H
#define MONSOON_OBJPIPE_DETAIL_ARRAYED_H

#include <monsoon/objpipe/detail/reader_intf.h>
#include <monsoon/objpipe/errc.h>
#include <deque>
#include <memory>
#include <initializer_list>

///\file
///\ingroup objpipe_detail

namespace monsoon {
namespace objpipe {
namespace detail {


/**
 * \brief A obj pipe that is supplied by a collection.
 * \ingroup objpipe_detail
 *
 * This implementation uses std::deque to store elements internally.
 * \tparam T \parblock
 *   The type of object emitted by the object pipe.
 *   \note The type may not be a reference, nor may it be const.
 * \endparblock
 * \tparam Alloc The allocator to use for the internal collection.
 * \sa \ref monsoon::objpipe::new_array()
 */
template<typename T, typename Alloc>
class arrayed
: public reader_intf<T>
{
 public:
  ///\copydoc reader_intf<T>::value_type
  using value_type = typename reader_intf<T>::value_type;
  ///\copydoc reader_intf<T>::pointer
  using pointer = typename reader_intf<T>::pointer;

 private:
  ///\brief The internal data type.
  using data_type = std::deque<value_type, Alloc>;

 public:
  ///\brief The allocator type.
  using allocator_type = typename data_type::allocator_type;

  /**
   * \brief Iteration constructor.
   *
   * \details
   * Create an arrayed reader_intf using an iterator pair.
   *
   * \param b,e Iterator range.
   * \param alloc An allocator, used to allocate storage for elements.
   */
  template<typename Iter>
  arrayed(Iter b, Iter e, allocator_type alloc = allocator_type())
  : data_(b, e, std::move(alloc))
  {}

  /**
   * \brief Iteration constructor.
   *
   * \details
   * Create an arrayed reader_intf using an iterator pair.
   *
   * \param il An initializer list to initialize the data.
   * \param alloc An allocator, used to allocate storage for elements.
   */
  arrayed(std::initializer_list<value_type> il,
      allocator_type alloc = allocator_type())
  : data_(il, std::move(alloc))
  {}

  ///\copydoc reader_intf<T>::is_pullable()
  auto is_pullable() const noexcept -> bool override {
    return !data_.empty();
  }

  ///\copydoc reader_intf<T>::wait()
  auto wait() const noexcept -> objpipe_errc override {
    return (data_.empty() ? objpipe_errc::closed : objpipe_errc::success);
  }

  ///\copydoc reader_intf<T>::empty()
  auto empty() const noexcept -> bool override {
    return data_.empty();
  }

  ///\copydoc reader_intf<T>::pull(objpipe_errc&)
  auto pull(objpipe_errc& e) -> std::optional<value_type> override {
    if (data_.empty()) {
      e = objpipe_errc::closed;
      return {};
    }

    e = objpipe_errc::success;
    auto result = std::optional<value_type>(std::move(data_.front()));
    data_.pop_front();
    return result;
  }

  ///\copydoc reader_intf<T>::pull()
  auto pull() -> value_type override {
    if (data_.empty()) {
      throw std::system_error(
          static_cast<int>(objpipe_errc::closed),
          objpipe_category());
    }

    value_type result = std::move(data_.front());
    data_.pop_front();
    return result;
  }

  ///\copydoc reader_intf<T>::try_pull(objpipe_errc&)
  auto try_pull(objpipe_errc& e) -> std::optional<value_type> override {
    return pull(e);
  }

  ///\copydoc reader_intf<T>::try_pull()
  auto try_pull() -> std::optional<value_type> override {
    objpipe_errc e;
    return try_pull(e);
  }

  ///\copydoc reader_intf<T>::front()
  auto front() const -> std::variant<pointer, objpipe_errc> override {
    using result_variant = std::variant<pointer, objpipe_errc>;
    if (data_.empty())
      return result_variant(std::in_place_index<1>, objpipe_errc::closed);
    return result_variant(
        std::in_place_index<0>,
        std::addressof(const_cast<std::add_lvalue_reference_t<value_type>>(data_.front())));
  }

  ///\copydoc reader_intf<T>::pop_front()
  auto pop_front() -> objpipe_errc override {
    if (data_.empty())
      return objpipe_errc::closed;
    data_.pop_front();
    return objpipe_errc::success;
  }

  ///\copydoc reader_intf<T>::add_continuation(std::unique_ptr<continuation_intf,writer_release>&&)
  void add_continuation(
      std::unique_ptr<continuation_intf, writer_release>&& c) override {
    // SKIP: no writer
  }

  ///\copydoc reader_intf<T>::erase_continuation(continuation_intf*)
  void erase_continuation(continuation_intf* c) override {
    // SKIP: no writer
  }

 private:
  ///\copydoc base_objpipe::on_last_reader_gone_()
  void on_last_reader_gone_() noexcept override {}
  ///\copydoc base_objpipe::on_last_writer_gone_()
  void on_last_writer_gone_() noexcept override {}

  ///\brief Sequence of elements to be emitted.
  data_type data_;
};


}}} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_ARRAYED_H */
