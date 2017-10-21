#ifndef MONSOON_OBJPIPE_WRITER_H
#define MONSOON_OBJPIPE_WRITER_H

///\file monsoon/objpipe/writer.h <monsoon/objpipe/writer.h
///\ingroup objpipe

#include <cassert>
#include <memory>
#include <type_traits>
#include <monsoon/objpipe/errc.h>
#include <monsoon/objpipe/detail/writer_intf.h>

namespace monsoon {
namespace objpipe {


/**
 * \brief An object pipe writer.
 * \ingroup objpipe
 *
 * \tparam T The type of objects written to the object pipe.
 */
template<typename T>
class writer
{
 public:
  /** @brief The type of objects in this object pipe. */
  using value_type = T;
  /** @brief Rvalue reference type for objects in this object pipe. */
  using rvalue_reference = std::add_rvalue_reference_t<value_type>;
  /** @brief const reference type for objects in this object pipe. */
  using rvalue_reference = std::add_lvalue_reference_t<std::add_const_t<value_type>>;

  /**
   * \brief Construct a writer using the given pointer.
   *
   * Mainly used internally to the objpipe library.
   */
  explicit writer(std::unique_ptr<detail::writer_intf<T>, detail::writer_release> ptr) noexcept
  : ptr_(std::move(ptr))
  {}

  ///@copydoc detail::writer_intf<T>::push(rvalue_reference,objpipe_errc&)
  void push(rvalue_reference v, objpipe_errc& e) {
    assert(ptr_ != nullptr);
    ptr_->push(std::move(v), e);
  }

  ///@copydoc detail::writer_intf<T>::push(const_reference,objpipe_errc&)
  void push(const_reference v, objpipe_errc& e) {
    assert(ptr_ != nullptr);
    ptr_->push(v, e);
  }

  ///@copydoc detail::writer_intf<T>::push(rvalue_reference)
  void push(rvalue_reference v) {
    assert(ptr_ != nullptr);
    ptr_->push(std::move(v));
  }

  ///@copydoc detail::writer_intf<T>::push(const_reference)
  void push(const_reference v) {
    assert(ptr_ != nullptr);
    ptr_->push(v);
  }

  /**
   * @return true iff the writer is valid and pushable.
   */
  explicit operator bool() const noexcept {
    return ptr_ != nullptr && ptr_->is_pushable();
  }

 private:
  std::unique_ptr<detail::writer_intf<T>, detail::writer_release> ptr_;
};


}} /* namespace monsoon::objpipe */

#endif /* MONSOON_OBJPIPE_WRITER_H */
