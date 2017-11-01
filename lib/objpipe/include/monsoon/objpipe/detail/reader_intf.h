#ifndef MONSOON_OBJPIPE_DETAIL_READER_INTF_H
#define MONSOON_OBJPIPE_DETAIL_READER_INTF_H

///\file
///\ingroup objpipe_detail

#include <monsoon/objpipe/errc.h>
#include <monsoon/objpipe/detail/base_objpipe.h>
#include <functional>
#include <optional>
#include <type_traits>
#include <variant>

namespace monsoon {
namespace objpipe {
namespace detail {


/**
 * \brief A continuation is an alternative to a reader.
 * \ingroup objpipe_detail
 *
 * When an object pipe has a continuation, it will keep it live
 * until its writer side is closed.
 */
class monsoon_objpipe_export_ continuation_intf
: public virtual base_objpipe
{
 public:
  ~continuation_intf() noexcept override;

 private:
  /**
   * copydoc base_objpipe::on_last_writer_gone_()
   *  The default for a continuation is to not care about writers,
   *  as all reading is done using forwarding to the source object pipe.
   */
  void on_last_writer_gone_() noexcept override;
  ///copydoc base_objpipe::on_last_writer_gone_()
  virtual void on_last_reader_gone_() noexcept override = 0;
};


/**
 * \brief This class is the interface type for the reader side of object pipe implementations.
 * \ingroup objpipe_detail
 * \tparam T The type of objects emitted by the object pipe.
 */
template<typename T>
class reader_intf
: public virtual base_objpipe
{
 public:
  /** Value type of elements in the object pipe. */
  using value_type = std::decay_t<T>;
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
   * Removes and returns an object from the pipe, if one is available.
   *
   * This call will synchronize as appropriate, but not wait for any writer to
   * push a value.
   * @param[out] e a variable for indicating success/failure conditions.
   * @return the pulled value, or an empty optional if none is available.
   */
  virtual auto try_pull(objpipe_errc& e) -> std::optional<value_type> = 0;

  /**
   * Removes and returns an object from the pipe, if one is available.
   *
   * This call will synchronize as appropriate, but not wait for any writer to
   * push a value.
   * @return the pulled value, or an empty optional if none is available.
   */
  virtual auto try_pull() -> std::optional<value_type> = 0;

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

  /**
   * Add a continuation.
   *
   * @note If the object pipe has no writer, this may be a noop.
   * @param c The continuation that is to be connected.
   */
  virtual void add_continuation(std::unique_ptr<continuation_intf, writer_release>&& c) = 0;

  /**
   * Remove a continuation.
   *
   * @note If the object pipe has no writer, this may be a noop.
   * @param c The continuation that is to be disconnected.
   */
  virtual void erase_continuation(continuation_intf* c) = 0;
};


}}} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_READER_INTF_H */
