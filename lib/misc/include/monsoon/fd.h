#ifndef FD_H
#define FD_H

#include <cstddef>
#include <string>
#include <monsoon/optional.h>
#include <monsoon/stream.h>

#if defined(WIN32)
# include <WinNT.h>
#endif

namespace monsoon {


class fd
: public stream_reader,
  public stream_writer
{
 public:
#if defined(WIN32)
  using implementation_type = HANDLE;
#else
  using implementation_type = int;
#endif
  enum open_mode {
    READ_ONLY,
    WRITE_ONLY,
    READ_WRITE
  };

  using size_type = std::uint64_t;
  using offset_type = size_type;

  fd() noexcept;
  fd(const fd&) = delete;
  fd(fd&&) noexcept;
  fd(const std::string& path, open_mode);
  ~fd() noexcept;

  static std::string normalize(const std::string&);

  void close() override;
  explicit operator bool() const noexcept;
  bool is_open() const noexcept { return static_cast<bool>(*this); }
  bool can_read() const noexcept;
  bool can_write() const noexcept;
  offset_type offset() const;
  optional<std::string> get_path() const;

  void flush();
  size_type size() const;

  std::size_t read(void*, std::size_t) override;
  std::size_t write(const void*, std::size_t) override;
  std::size_t read_at(offset_type, void*, std::size_t);
  std::size_t write_at(offset_type, const void*, std::size_t);

  void swap(fd&) noexcept;

  implementation_type underlying() const noexcept { return handle_; }

 private:
#if defined(WIN32)
  implementation_type handle_;
#else
  implementation_type handle_;
  std::string fname_;
#endif

  open_mode mode_;
};

inline void swap(fd& x, fd& y) noexcept {
  x.swap(y);
}


} /* namespace monsoon */

#endif /* FD_H */
