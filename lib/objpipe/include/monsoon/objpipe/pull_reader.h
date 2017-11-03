#ifndef MONSOON_OBJPIPE_PULL_READER_H
#define MONSOON_OBJPIPE_PULL_READER_H

#include <monsoon/objpipe/detail/reader_intf.h>
#include <monsoon/objpipe/reader.h>
#include <monsoon/objpipe/errc.h>
#include <variant>
#include <optional>
#include <cassert>

namespace monsoon {
namespace objpipe {


enum no_value_reason {
  END_OF_DATA, ///< Indicate that no more data is available.
  TEMPORARY ///< Indicate more data may come available at a later time.
};

template<typename T>
class pull_reader
: public detail::reader_intf<T>
{
  static_assert(!std::is_reference_v<T>
      && !std::is_const_v<T>
      && !std::is_volatile_v<T>,
      "pull_reader<T> expects a non-const, non-volatile, non-reference type.");

 public:
  using value_type = typename detail::reader_intf<T>::value_type;
  using pointer = typename detail::reader_intf<T>::pointer;

 protected:
  pull_reader() = default;

 public:
  ~pull_reader() noexcept override {}

  auto is_pullable() const noexcept -> bool override final {
    if (pending_.has_value() || ex_pending_ != nullptr)
      return true;
    try {
      return is_pullable_impl();
    } catch (...) {
      const_cast<pull_reader&>(*this).ex_pending_ = std::current_exception();
      return true;
    }
  }

  auto wait() const noexcept -> objpipe_errc override {
    for (;;) {
      if (pending_.has_value() || ex_pending_ != nullptr)
        return objpipe_errc::success;

      try {
        auto wait_result = wait_for_data();
        if (wait_result.has_value()) {
          switch (*wait_result) {
            case END_OF_DATA:
              return objpipe_errc::closed;
            case TEMPORARY:
              break;
          }
        }

        auto next = const_cast<pull_reader&>(*this).try_next();
        if (next.index() == 0) {
          const_cast<pull_reader&>(*this).pending_ =
              std::get<0>(std::move(next));
        } else {
          switch (std::get<1>(next)) {
            case END_OF_DATA:
              return objpipe_errc::closed;
            case TEMPORARY:
              break;
          }
        }
      } catch (...) {
        const_cast<pull_reader&>(*this).ex_pending_ = std::current_exception();
      }
    }
  }

  auto empty() const noexcept -> bool override final {
    if (pending_.has_value() || ex_pending_ != nullptr)
      return false;
    try {
      return empty_impl();
    } catch (...) {
      const_cast<pull_reader&>(*this).ex_pending_ = std::current_exception();
      return false;
    }
  }

  auto pull(objpipe_errc& e) -> std::optional<value_type> override final {
    std::optional<value_type> result;
    for (result = try_pull(e);
        e == objpipe_errc::success && !result.has_value();
        result = try_pull(e)) {
      try {
        auto wait_result = wait_for_data();
        if (wait_result.has_value()) {
          switch (*wait_result) {
            case END_OF_DATA:
              e = objpipe_errc::closed;
              return result;
            case TEMPORARY:
              break;
          }
        }
      } catch (...) {
        ex_pending_ = std::current_exception();
        throw;
      }
    }

    assert(result.has_value() == (e == objpipe_errc::success));
    return result;
  }

  auto pull() -> value_type override final {
    objpipe_errc e;
    auto result = pull(e);
    if (e != objpipe_errc::success)
      throw std::system_error(static_cast<int>(e), objpipe_category());

    assert(result.has_value());
    return *std::move(result);
  }

  auto try_pull(objpipe_errc& e) -> std::optional<value_type> override final {
    if (ex_pending_ != nullptr)
      std::rethrow_exception(ex_pending_);
    e = objpipe_errc::success;

    try {
      if (pending_.has_value()) {
        std::optional<value_type> result = std::move(pending_);
        pending_.reset();
        return result;
      }

      auto next = try_next();
      if (next.index() == 0)
        return std::get<0>(std::move(next));
      switch (std::get<1>(next)) {
        case END_OF_DATA:
          e = objpipe_errc::closed;
          break;
        case TEMPORARY:
          break;
      }
      return {};
    } catch (...) {
      ex_pending_ = std::current_exception();
      throw;
    }
  }

  auto try_pull() -> std::optional<value_type> override final {
    objpipe_errc e;
    std::optional<value_type> result = try_pull(e);

    if (e != objpipe_errc::success) {
      assert(!result.has_value());
      throw std::system_error(static_cast<int>(e), objpipe_category());
    }
    return result;
  }

  auto front() const -> std::variant<pointer, objpipe_errc> override final {
    if (!pending_.has_value()) {
      objpipe_errc e;
      const_cast<pull_reader&>(*this).pending_ =
          const_cast<pull_reader&>(*this).pull(e);
      if (e != objpipe_errc::success) {
        assert(!pending_);
        return e;
      }
    }
    return std::variant<pointer, objpipe_errc>(
        std::in_place_index<0>,
        std::addressof(*const_cast<pull_reader&>(*this).pending_));
  }

  auto pop_front() -> objpipe_errc override final {
    if (pending_.has_value()) {
      pending_.reset();
      return objpipe_errc::success;
    }

    objpipe_errc e;
    pull(e); // Discard result.
    return e;
  }

  void add_continuation(
      std::unique_ptr<detail::continuation_intf, detail::writer_release>&& c)
      override
  {}

  void erase_continuation(detail::continuation_intf* c) override {}

 private:
  virtual auto try_next() -> std::variant<T, no_value_reason> = 0;
  virtual auto wait_for_data() const -> std::optional<no_value_reason> = 0;

  virtual auto is_pullable_impl() const -> bool {
    assert(!pending_.has_value());
    objpipe_errc e;
    const_cast<pull_reader&>(*this).pending_ =
        const_cast<pull_reader&>(*this).try_pull(e);
    return e == objpipe_errc::success;
  }

  virtual auto empty_impl() const -> bool {
    auto next = const_cast<pull_reader&>(*this).try_next();
    if (next.index() == 0) {
      const_cast<pull_reader&>(*this).pending_ = std::get<0>(std::move(next));
      return false;
    } else {
      return true;
    }
  }

  void on_last_reader_gone_() noexcept override {}
  void on_last_writer_gone_() noexcept override final {}

  std::optional<T> pending_;
  std::exception_ptr ex_pending_;
};


template<typename PullReaderImpl, typename... Args>
auto new_pull_reader(Args&&... args)
-> reader<typename PullReaderImpl::value_type> {
  using namespace ::monsoon::objpipe::detail;

  return reader<typename PullReaderImpl::value_type>(
      reader_release::link(new PullReaderImpl(std::forward<Args>(args)...)));
}


}} /* namespace monsoon::objpipe */

#endif /* MONSOON_OBJPIPE_PULL_READER_H */
