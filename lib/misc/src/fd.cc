#include <monsoon/fd.h>

#if defined(WIN32)

#include <Windows.h>
#include <utility>
#include <vector>

namespace monsoon {
namespace {


static void throw_last_error_() {
  const DWORD dwErrVal = GetLastError();
  const std::error_code ec(dwErrVal, std::generic_category());
  throw std::system_error(ec, "File IO error");
}


} /* namespace monsoon::<anonymous> */


fd::fd() noexcept
: handle_(INVALID_HANDLE_VALUE)
{}

fd::fd(fd&& o) noexcept
: handle_(std::exchange(o.handle_, INVALID_HANLE_VALUE))
{}

fd::fd(const std::string& fname)
: fd()
{
  DWORD dwDesiredAccess = 0;
  switch (mode) {
    case READ_ONLY:
      dwDesiredAccess |= GENERIC_READ;
      break;
    case WRITE_ONLY:
      dwDesiredAccess |= GENERIC_WRITE;
      break;
    case READ_WRITE:
      dwDesiredAccess |= GENERIC_READ | GENERIC_WRITE;
      break;
  }

  handle_ = CreateFile(
      fname.c_str(),
      dwDesiredAccess,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      nullptr,
      OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
      nullptr);
  if (handle_ == INVALID_HANDLE_VALUE) throw_last_error_();
}

fd::~fd() noexcept {
  close();
}

void fd::close() {
  if (handle_ != INVALID_HANDLE_VALUE) {
    CloseHandle(handle_);
    handle_ = INVALID_HANDLE_VALUE;
  }
}

fd::operator bool() const noexcept {
  return handle_ != INVALID_HANDLE_VALUE;
}

optional<std::string> fd::get_path() const {
  if (handle_ == INVALID_HANDLE_VALUE)
    return {};

#if __cplusplus >= 201703L
  std::string buffer;
#else
  std::vector<char> buffer; // Need modifiably data() pointer.
#endif
  buffer.resize(1024);
  auto pathlen = GetFinalPathNameByHandle(handle_, buffer.data(), buffer.size(), FILE_NAME_NORMALIZED);
  if (pathlen > buffer.size()) {
    buffer.resize(pathlen);
    pathlen = GetFinalPathNameByHandle(handle_, buffer.data(), buffer.size(), FILE_NAME_NORMALIZED);
  }
  if (pathlen == 0) throw_last_error_();
  buffer.resize(pathlen);

  if (buffer.empty())
    return {};

#if __cplusplus >= 201703L
  return buffer;
#else
  return std::string(buffer.begin(), buffer.end());
#endif
}

void fd::flush() {
  if (!FlushFileBuffers(handle_)) throw_last_error_();
}

auto fd::size() const -> size_type {
  LARGE_INTEGER v;
  if (!GetFileSizeEx(handle_, &v)) throw_last_error_();
  return v;
}

void fd::swap(fd& o) noexcept {
  std::swap(handle_, o.handle_);
}


} /* namespace monsoon::history */

#else

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cerrno>
#include <utility>
#include <system_error>

namespace monsoon {
namespace {


static void throw_errno_() {
  std::system_error(errno, std::system_category());
}


} /* namespace monsoon::<anonymous> */

fd::fd() noexcept
: fd_(-1)
{}

fd::fd(fd&& o) noexcept
: fd_(std::exchange(o.fd_, -1))
{}

fd::fd(const std::string& fname, open_mode mode)
: fd_(-1),
  fname_(normalize(fname))
{
  int fl = 0;
  switch (mode) {
    case READ_ONLY:
      fl |= O_RDONLY;
      break;
    case WRITE_ONLY:
      fl |= O_WRONLY;
      break;
    case READ_WRITE:
      fl |= O_RDWR;
      break;
  }

  fd_ = ::open(fname_.c_str(), fl);
  if (fd_ == -1) throw_errno_();
}

fd::~fd() noexcept {
  close();
}

void fd::close() {
  if (fd_ != -1) {
    ::close(fd_);
    fd_ = -1;
  }
}

fd::operator bool() const noexcept {
  return fd_ != -1;
}

optional<std::string> fd::get_path() const {
  if (fd_ == -1 || fname_.empty())
    return {};
  return fname_;
}

void fd::flush() {
  if (fsync(fd_)) throw_errno_();
}

auto fd::size() const -> size_type {
  struct stat sb;

  if (fstat(fd_, &sb)) throw_errno_();
  return sb.st_size;
}

void fd::swap(fd& o) noexcept {
  std::swap(fname_, o.fname_);
  std::swap(fd_, o.fd_);
}


} /* namespace monsoon */

#endif


#ifdef HAS_CXX_FILESYSTEM
# include <filesystem>
#else
# include <boost/filesystem.hpp>
#endif

namespace monsoon {


std::string fd::normalize(const std::string& fname) {
#if HAS_CXX_FILESYSTEM
  namespace filesystem = ::std::filesystem;
#else
  namespace filesystem = ::boost::filesystem;
#endif

  filesystem::path path = fname;
  return boost::filesystem::canonical(path).native();
}


} /* namespace monsoon */
