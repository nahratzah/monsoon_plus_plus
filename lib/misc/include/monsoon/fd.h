#ifndef FD_H
#define FD_H

#include <string>
#include <monsoon/optional.h>

namespace monsoon {


class fd {
 public:
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

  void close();
  explicit operator bool() const noexcept;
  bool can_read() const noexcept;
  bool can_write() const noexcept;
  offset_type offset() const;
  optional<std::string> get_path() const;

  void flush();
  size_type size() const;

  void swap(fd&) noexcept;

 private:
#if defined(WIN32)
  HANDLE handle_;
#else
  int fd_;
  std::string fname_;
#endif

  open_mode mode_;
};

inline void swap(fd& x, fd& y) noexcept {
  x.swap(y);
}


} /* namespace monsoon */

#endif /* FD_H */
