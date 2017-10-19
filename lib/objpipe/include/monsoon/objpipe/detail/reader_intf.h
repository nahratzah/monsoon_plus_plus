#ifndef MONSOON_OBJPIPE_DETAIL_READER_INTF_H
#define MONSOON_OBJPIPE_DETAIL_READER_INTF_H

///@file monsoon/objpipe/detail/reader_intf.h <monsoon/objpipe/detail/reader_intf.h>

#include <monsoon/objpipe/errc.h>
#include <monsoon/objpipe/detail/base_objpipe.h>
#include <variant>
#include <optional>
#include <type_traits>

namespace monsoon {
namespace objpipe {
namespace detail {


/**
 * This class is the interface type for the reader side of object pipe implementations.
 * @headerfile "" <monsoon/objpipe/detail/reader_intf.h>
 */
template<typename T>
class reader_intf<T>
: public virtual base_objpipe
{
 public:
  static_assert(!std::is_reference_v<T>,
      "Reference queues are not implemented.");

  /** Value type of elements in the object pipe. */
  using value_type = T;
  /** Pointer type of elements in the object pipe. */
  using pointer = std::add_pointer_t<value_type>;

  /**
   * Test if the object pipe is pullable.
   *
   * An object pipe is pullable if any of the following is true:
   * - it is not empty
   * - it has a writer attached
   * @return true if the pipe can be pulled from.
   */
  virtual auto is_pullable() const noexcept -> bool = 0;

  /**
   * Block until an element becomes available.
   * @return an error condition indicating success or failure.
   */
  virtual auto wait() const noexcept -> objpipe_errc = 0;

  /**
   * Test if the pipe has elements available.
   *
   * Note that a pipe can transition from empty to not-empty
   * by a writer adding an object.
   * @return true if the are no elements available.
   */
  virtual auto empty() const noexcept -> bool = 0;

  /**
   * Pull an object from the pipe.
   * @param[out] e a variable for indicating success/failure conditions.
   * @return an optional, containing the pulled value iff successful.
   */
  virtual auto pull(objpipe_errc& e) -> std::optional<value_type> = 0;

  /**
   * Removes and returns an object from the pipe.
   * @throw std::system_error if the pipe is empty and has no writers connected.
   * @return the pulled value.
   */
  virtual auto pull() -> value_type = 0;

  /**
   * Acquire a reference to the next value in the pipe, without removing it.
   * @return a variant with a pointer upon success, or an indication of failure.
   */
  virtual auto front() const -> std::variant<pointer, objpipe_errc> = 0;

  /**
   * Remove the next value from the pipe.
   * @return a value indicating success or failure.
   */
  virtual auto pop_front() -> objpipe_errc = 0;
};


}}} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_READER_INTF_H */
