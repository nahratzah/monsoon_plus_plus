#ifndef AIO_H
#define AIO_H

#include <monsoon/misc_export_.h>
#include <monsoon/io/fd.h>
#include <cstddef>

#if !defined(WIN32) && __has_include(<aio.h>)
# include <aio.h>

# include <cstring>
# include <memory>
# include <optional>
# include <vector>
#else
# include <unordered_map>
#endif

namespace monsoon::io {


class monsoon_misc_export_ aio {
  public:
  class fd_target;
  class const_fd_target;

  aio() = default;
  ~aio() noexcept;

  auto on(fd& f) -> fd_target;
  auto on(const fd& f) -> const_fd_target;

  void start();
  void join();
  void start_and_join();

#if !defined(WIN32) && __has_include(<aio.h>)
  class datum {
    friend aio;

    public:
    datum()
    : ptr_(std::make_unique<aiocb>())
    {
      std::memset(ptr_.get(), 0, sizeof(aiocb));
    }

    auto operator*() -> aiocb& { return *ptr_; }
    auto operator*() const -> const aiocb& { return *ptr_; }
    auto operator->() -> aiocb* { return ptr_.get(); }
    auto operator->() const -> const aiocb* { return ptr_.get(); }

    private:
    std::unique_ptr<aiocb> ptr_;
  };

  class flush_datum {
    friend aio;

    public:
    flush_datum(fd& f, bool data_only)
    : ptr_(std::make_unique<aiocb>()),
      data_only_(data_only)
    {
      std::memset(ptr_.get(), 0, sizeof(aiocb));
      ptr_->aio_fildes = f.underlying();
    }

    private:
    auto execute() const -> aiocb*;

    std::unique_ptr<aiocb> ptr_;
    bool data_only_;
  };

  private:
  void add_datum(datum&& d);
  void add_datum(flush_datum&& d);
  auto build_aiocb_vector_() const -> std::vector<aiocb*>;
  void cancel_recover_() noexcept;

  std::vector<aiocb*> aiocb_vector_;
  std::vector<datum> datum_vector_;
  std::vector<flush_datum> flush_vector_;
#else
  private:
  std::unordered_map<fd*, bool> flush_map_;
#endif
};


class aio::fd_target {
  friend aio;

  private:
  fd_target(aio& a, fd& f) noexcept
  : fd_(&f),
    aio_(&a)
  {}

  public:
  constexpr fd_target() noexcept = default;

  void read_at(monsoon::io::fd::offset_type off, void* buf, std::size_t buflen);
  void write_at(monsoon::io::fd::offset_type off, const void* buf, std::size_t buflen);
  void flush(bool data_only = false);

  private:
  fd* fd_ = nullptr;
  aio* aio_ = nullptr;
};


class aio::const_fd_target {
  friend aio;

  private:
  const_fd_target(aio& a, const fd& f) noexcept
  : fd_(&f),
    aio_(&a)
  {}

  public:
  constexpr const_fd_target() noexcept = default;

  const_fd_target(const fd_target& x)
  : fd_(x.fd_),
    aio_(x.aio_)
  {}

  void read_at(monsoon::io::fd::offset_type off, void* buf, std::size_t buflen);

  private:
  const fd* fd_ = nullptr;
  aio* aio_ = nullptr;
};


inline auto aio::on(fd& f) -> fd_target {
  return fd_target(*this, f);
}

inline auto aio::on(const fd& f) -> const_fd_target {
  return const_fd_target(*this, f);
}

#if defined(WIN32) || !__has_include(<aio.h>)
inline aio::~aio() noexcept = default;

inline void aio::join() {}

inline void aio::start_and_join() {
  start();
  join();
}
#endif


} /* namespace monsoon::io */

#endif /* AIO_H */
