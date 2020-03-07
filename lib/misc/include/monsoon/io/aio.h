#ifndef AIO_H
#define AIO_H

#include <monsoon/misc_export_.h>
#include <monsoon/io/fd.h>
#include <cstddef>
#include <unordered_map>

#if !defined(WIN32) && __has_include(<aio.h>)
# include <aio.h>

# include <cstring>
# include <memory>
# include <optional>
# include <vector>
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

  private:
#if !defined(WIN32) && __has_include(<aio.h>)
  class op_
  : public ::aiocb
  {
    friend aio;
    friend fd_target;
    friend const_fd_target;

    protected:
    explicit op_(fd& f) noexcept;

    public:
    virtual ~op_() noexcept;

    ///\returns True if the aio has been re-enqueued.
    bool on_len(std::size_t len);

    protected:
    void reset_() noexcept;
    void enqueue_();

    private:
    virtual void do_on_len(std::size_t len) = 0;

    bool started_ = false;
    fd& f;
  };

  class read_op_;
  class write_op_;
  class flush_op_;

  class flush_barrier {
    public:
    constexpr flush_barrier() noexcept
    : wait_count(0),
      dataonly(false)
    {}

    constexpr flush_barrier(std::size_t wait_count, bool dataonly) noexcept
    : wait_count(wait_count),
      dataonly(dataonly)
    {}

    std::size_t wait_count = 0u;
    bool dataonly = false;
  };

  bool started_ = false;
  std::vector<std::unique_ptr<op_>> ops_;
  std::unordered_map<fd*, flush_barrier> flush_map_;
#else
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
