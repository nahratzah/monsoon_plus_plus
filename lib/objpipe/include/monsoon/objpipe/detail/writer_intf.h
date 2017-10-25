#ifndef MONSOON_OBJPIPE_DETAIL_WRITER_INTF_H
#define MONSOON_OBJPIPE_DETAIL_WRITER_INTF_H

///\file
///\ingroup objpipe_detail

#include <monsoon/objpipe/errc.h>
#include <monsoon/objpipe/detail/base_objpipe.h>

namespace monsoon {
namespace objpipe {
namespace detail {


/**
 * \brief This class is the interface to the writer side of the object pipe implementation.
 * \ingroup objpipe_detail
 * \tparam T The type accepted by the object pipe.
 */
template<typename T>
class writer_intf
: public virtual base_objpipe
{
 public:
  static_assert(!std::is_reference_v<T>,
      "It makes no sense to write references to an objpipe.");
  static_assert(!std::is_const_v<T>,
      "It makes no sense to write const values to an objpipe.");

  ///@copydoc reader_intf<T>::value_type
  using value_type = T;
  ///@copydoc reader_intf<T>::pointer
  using pointer = std::add_pointer_t<value_type>;
  /** Rvalue reference type. */
  using rvalue_reference = std::add_rvalue_reference_t<value_type>;
  /** Const reference type. */
  using const_reference = std::add_lvalue_reference_t<std::add_const_t<value_type>>;

  /**
   * Test if the object pipe is pushable.
   *
   * An object pipe is pushable if:
   * - it has a reader attached.
   */
  virtual auto is_pushable() const noexcept -> bool = 0;

  /**
   * Push an object onto the pipe.
   * @param[in] v the value that is to be pushed.
   * @param[out] e a variable for indication success/failure conditions.
   */
  virtual void push(rvalue_reference v, objpipe_errc& e) = 0;

  /**
   * Push an object onto the pipe.
   * @param[in] v
   * @parblock
   *   the value that is to be pushed.
   *   @note A copy of v will be made.
   * @endparblock
   * @param[out] e a variable for indication success/failure conditions.
   */
  virtual void push(const_reference v, objpipe_errc& e) {
    push(value_type(v), e);
  }

  /**
   * Push an object onto the pipe.
   * @param[in] v the value that is to be pushed.
   * @throw std::system_error if the push operation fails.
   */
  virtual void push(rvalue_reference v) {
    objpipe_errc e;
    push(std::move(v), e);
    if (e) throw std::system_error(static_cast<int>(e), objpipe_category());
  }

  /**
   * Push an object onto the pipe.
   * @param[in] v
   * @parblock
   *   the value that is to be pushed.
   *   @note A copy of v will be made.
   * @endparblock
   * @throw std::system_error if the push operation fails.
   */
  virtual void push(const_reference v) {
    objpipe_errc e;
    push(v, e);
    if (e) throw std::system_error(static_cast<int>(e), objpipe_category());
  }
};


}}} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_WRITER_INTF_H */
