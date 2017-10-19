#ifndef MONSOON_OBJPIPE_DETAIL_BASE_OBJPIPE_H
#define MONSOON_OBJPIPE_DETAIL_BASE_OBJPIPE_H

///@file monsoon/objpipe/detail/base_objpipe.h <monsoon/objpipe/detail/base_objpipe.h>

#include <monsoon/objpipe/objpipe_export_.h>
#include <atomic>
#include <cstdint>

namespace monsoon {
namespace objpipe {
namespace detail {


/**
 * Base class for all objpipe instances.
 * @headerfile "" <monsoon/objpipe/detail/base_objpipe.h>
 */
class monsoon_objpipe_export_ base_objpipe {
  friend struct reader_release;
  friend struct writer_release;

 protected:
  /** Constructible by derived classes. */
  base_objpipe() noexcept {}

 private:
  base_objpipe(const base_objpipe&) = delete;
  base_objpipe(base_objpipe&&) = delete;
  base_objpipe& operator=(const base_objpipe&) = delete;
  base_objpipe& operator=(base_objpipe&&) = delete;

 public:
  /** Object pipes are destructible. */
  virtual ~base_objpipe() noexcept;

 private:
  /** Function invoked when the last reader closes. */
  virtual void on_last_reader_gone_() noexcept = 0;
  /** Function invoked when the last writer closes. */
  virtual void on_last_writer_gone_() noexcept = 0;

  mutable std::atomic<std::size_t> refcnt_{ std::size_t(0u) },
                                   writer_refcnt_{ std::size_t(0u) },
                                   reader_refcnt_{ std::size_t(0u) };
};

/**
 * Functor used in combination with std::unique_ptr, to release reader references.
 * @headerfile "" <monsoon/objpipe/detail/base_objpipe.h>
 */
struct monsoon_objpipe_export_ reader_release {
  /**
   * Release functor, releases the reader side of an objpipe.
   * @param ptr Pointer to objpipe to be released.
   */
  void operator()(base_objpipe* ptr) const noexcept;
};

/**
 * Functor used in combination with std::unique_ptr, to release reader references.
 * @headerfile "" <monsoon/objpipe/detail/base_objpipe.h>
 */
struct monsoon_objpipe_export_ writer_release {
  /**
   * Release functor, releases the writer side of an objpipe.
   * @param ptr Pointer to objpipe to be released.
   */
  void operator()(base_objpipe* ptr) const noexcept;
};


}}} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_BASE_OBJPIPE_H */
