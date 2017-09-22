#include <monsoon/io/fd.h>

#if defined(WIN32)

#include <Windows.h>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

namespace monsoon {
namespace io {
namespace {


static void throw_last_error_() {
  const DWORD dwErrVal = GetLastError();
  const std::error_code ec(dwErrVal, std::generic_category());
  throw std::system_error(ec, "File IO error");
}


} /* namespace monsoon::io::<anonymous> */


fd::fd() noexcept
: handle_(INVALID_HANDLE_VALUE)
{}

fd::fd(fd&& o) noexcept
: handle_(std::exchange(o.handle_, INVALID_HANLE_VALUE))
{}

fd::fd(const std::string& fname, open_mode mode)
: handle_(INVALID_HANDLE_VALUE),
  mode_(mode)
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
      FILE_ATTRIBUTE_NORMAL,
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

auto fd::offset() const -> offset_type {
  LARGE_INTEGER new_off;
  if (!SetFilePointerEx(handle_, 0, &new_off, FILE_CURRENT))
    throw_last_error_();
  return new_off;
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

auto fd::read(void* buf, std::size_t nbytes) -> std::size_t {
  if (nbytes > std::numeric_limits<DWORD>::max())
    nbytes = std::numeric_limits<DWORD>::max();

  DWORD rlen;
  if (!ReadFile(handle_, buf, nbytes, &rlen, nullptr)) throw_last_error_();
  return rlen;
}

auto fd::write(const void* buf, std::size_t nbytes) -> std::size_t {
  if (nbytes > std::numeric_limits<DWORD>::max())
    nbytes = std::numeric_limits<DWORD>::max();

  DWORD wlen;
  if (!WriteFile(handle_, buf, nbytes, &wlen, nullptr)) throw_last_error_();
  return wlen;
}

inline OVERLAPPED make_overlapped(fd::offset_type off) {
  using u_DWORD = std::make_unsigned_t<DWORD>;
  using u_DWORD_lims = std::numeric_limits<u_DWORD>;

  OVERLAPPED overlapped;
  overlapped.Internal = nullptr;
  overlapped.InternalHigh = nullptr;
  overlapped.Offset = static_cast<u_DWORD>(off);
  overlapped.OffsetHigh = static_cast<u_DWORD>(off >> u_DWORD_lims::digits);
  overlapped.hEvent = INVALID_HANDLE_VALUE;
  return overlapped;
}

auto fd::read_at(offset_type off, void* buf, std::size_t nbytes) const
-> std::size_t {
  if (nbytes > std::numeric_limits<DWORD>::max())
    nbytes = std::numeric_limits<DWORD>::max();
  OVERLAPPED ol_data = make_overlapped(off);

  DWORD rlen;
  if (!ReadFile(handle_, buf, nbytes, &rlen, &ol_data)) throw_last_error_();
  return rlen;
}

auto fd::write_at(offset_type off, const void* buf, std::size_t nbytes)
-> std::size_t {
  if (nbytes > std::numeric_limits<DWORD>::max())
    nbytes = std::numeric_limits<DWORD>::max();
  OVERLAPPED ol_data = make_overlapped(off);

  DWORD wlen;
  if (!WriteFile(handle_, buf, nbytes, &wlen, &ol_data)) throw_last_error_();
  return wlen;
}

void fd::swap(fd& o) noexcept {
  using std::swap;

  swap(handle_, o.handle_);
  swap(mode_, o.mode_);
}


}} /* namespace monsoon::io */

#else

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cerrno>
#include <utility>
#include <system_error>

namespace monsoon {
namespace io {
namespace {


static void throw_errno_() {
  throw std::system_error(errno, std::system_category());
}


} /* namespace monsoon::io::<anonymous> */

fd::fd() noexcept
: handle_(-1)
{}

fd::fd(fd&& o) noexcept
: handle_(std::exchange(o.handle_, -1))
{}

fd::fd(const std::string& fname, open_mode mode)
: handle_(-1),
  fname_(normalize(fname)),
  mode_(mode)
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

  handle_ = ::open(fname_.c_str(), fl);
  if (handle_ == -1) throw_errno_();
}

fd::~fd() noexcept {
  close();
}

void fd::close() {
  if (handle_ != -1) {
    ::close(handle_);
    handle_ = -1;
  }
}

fd::operator bool() const noexcept {
  return handle_ != -1;
}

auto fd::offset() const -> offset_type {
  const auto off = lseek(handle_, 0, SEEK_CUR);
  if (off == -1) throw_errno_();
  return off;
}

optional<std::string> fd::get_path() const {
  if (handle_ == -1 || fname_.empty())
    return {};
  return fname_;
}

void fd::flush() {
  if (fsync(handle_)) throw_errno_();
}

auto fd::size() const -> size_type {
  struct stat sb;

  if (fstat(handle_, &sb)) throw_errno_();
  return sb.st_size;
}

auto fd::read(void* buf, std::size_t nbytes) -> std::size_t {
  const auto rlen = ::read(handle_, buf, nbytes);
  if (rlen == -1) throw_errno_();
  return rlen;
}

auto fd::write(const void* buf, std::size_t nbytes) -> std::size_t {
  const auto rlen = ::write(handle_, buf, nbytes);
  if (rlen == -1) throw_errno_();
  return rlen;
}

auto fd::read_at(offset_type off, void* buf, std::size_t nbytes) const
-> std::size_t {
  const auto rlen = ::pread(handle_, buf, nbytes, off);
  if (rlen == -1) throw_errno_();
  return rlen;
}

auto fd::write_at(offset_type off, const void* buf, std::size_t nbytes)
-> std::size_t {
  const auto rlen = ::pwrite(handle_, buf, nbytes, off);
  if (rlen == -1) throw_errno_();
  return rlen;
}

void fd::swap(fd& o) noexcept {
  using std::swap;

  swap(fname_, o.fname_);
  swap(handle_, o.handle_);
  swap(mode_, o.mode_);
}


}} /* namespace monsoon::io */

#endif


#ifdef HAS_CXX_FILESYSTEM
# include <filesystem>
#else
# include <boost/filesystem.hpp>
#endif

namespace monsoon {
namespace io {


std::string fd::normalize(const std::string& fname) {
#if HAS_CXX_FILESYSTEM
  namespace filesystem = ::std::filesystem;
#else
  namespace filesystem = ::boost::filesystem;
#endif

  filesystem::path path = fname;
  return boost::filesystem::canonical(path).native();
}

bool fd::can_read() const noexcept {
  return *this && (mode_ == READ_ONLY || mode_ == READ_WRITE);
}

bool fd::can_write() const noexcept {
  return *this && (mode_ == WRITE_ONLY || mode_ == READ_WRITE);
}

bool fd::at_end() const {
  return offset() == size();
}


}} /* namespace monsoon::io */
