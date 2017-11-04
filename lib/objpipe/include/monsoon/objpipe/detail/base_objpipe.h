#ifndef MONSOON_OBJPIPE_DETAIL_BASE_OBJPIPE_H
#define MONSOON_OBJPIPE_DETAIL_BASE_OBJPIPE_H

///\file
///\ingroup objpipe_detail

#include <monsoon/objpipe/objpipe_export_.h>
#include <atomic>
#include <cstdint>
#include <memory>

namespace monsoon {
namespace objpipe {
namespace detail {


/**
 * \brief Base class for all objpipe instances.
 * \ingroup objpipe_detail
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

 protected:
  /** @return True iff there is a reader connected. */
  bool has_reader() const noexcept {
    return reader_refcnt_.load(std::memory_order_acquire) > 0u;
  }

  /** @return True iff there is a writer connected. */
  bool has_writer() const noexcept {
    return writer_refcnt_.load(std::memory_order_acquire) > 0u;
  }

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
 * \brief Functor used in combination with std::unique_ptr, to release reader references.
 * \ingroup objpipe_detail
 */
struct monsoon_objpipe_export_ reader_release {
  /**
   * Create a unique pointer for the linked reader implementation.
   * @param r The reader to create a pointer for.
   * @return A unique pointer holding the reader.
   */
  template<typename T>
  static auto link(T* r) noexcept
  -> std::enable_if_t<
        std::is_base_of_v<base_objpipe, T>,
        std::unique_ptr<T, reader_release>> {
    r->refcnt_.fetch_add(1u, std::memory_order_acquire);
    r->reader_refcnt_.fetch_add(1u, std::memory_order_acquire);
    return std::unique_ptr<T, reader_release>(r);
  }

  /**
   * Release functor, releases the reader side of an objpipe.
   * @param ptr Pointer to objpipe to be released.
   */
  void operator()(base_objpipe* ptr) const noexcept;
};

/**
 * \brief Functor used in combination with std::unique_ptr, to release reader references.
 * \ingroup objpipe_detail
 */
struct monsoon_objpipe_export_ writer_release {
  /**
   * Create a unique pointer for the linked writer implementation.
   * @param w The writer to create a pointer for.
   * @return A unique pointer holding the writer.
   */
  template<typename T>
  static auto link(T* w) noexcept
  -> std::enable_if_t<
        std::is_base_of_v<base_objpipe, T>,
        std::unique_ptr<T, writer_release>> {
    w->refcnt_.fetch_add(1u, std::memory_order_acquire);
    w->writer_refcnt_.fetch_add(1u, std::memory_order_acquire);
    return std::unique_ptr<T, writer_release>(w);
  }

  /**
   * Release functor, releases the writer side of an objpipe.
   * @param ptr Pointer to objpipe to be released.
   */
  void operator()(base_objpipe* ptr) const noexcept;
};


}}} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_BASE_OBJPIPE_H */
