#include <monsoon/io/fd.h>

#if __has_include(<filesystem>)
# include <filesystem>
#else
# include <boost/filesystem.hpp>
#endif


#if defined(WIN32)

#include <Windows.h>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

namespace monsoon {
namespace io {
namespace {


static void throw_last_error_(DWORD dwErrVal) {
  const std::error_code ec(dwErrVal, std::generic_category());
  throw std::system_error(ec, "File IO error");
}

static void throw_last_error_() {
  throw_last_error_(GetLastError());
}


} /* namespace monsoon::io::<anonymous> */


fd::fd() noexcept
: handle_(INVALID_HANDLE_VALUE)
{}

fd::fd(fd&& o) noexcept
: handle_(std::exchange(o.handle_, INVALID_HANLE_VALUE)),
  mode_(std::move(o.mode_))
{}

fd& fd::operator=(fd&& o) noexcept {
  using std::swap;

  close();
  swap(handle_, o.handle_);
  mode_ = std::move(o.mode_);
  return *this;
}

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

fd fd::create(const std::string& fname, open_mode mode) {
  fd new_fd;

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

  new_fd.handle_ = CreateFile(
      fname.c_str(),
      dwDesiredAccess,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      nullptr,
      CREATE_NEW,
      FILE_ATTRIBUTE_NORMAL,
      nullptr);
  if (new_fd.handle_ == INVALID_HANDLE_VALUE) throw_last_error_();
  new_fd.mode_ = READ_WRITE;
  return new_fd;
}

fd fd::tmpfile(const std::string& prefix) {
#if  __has_include(<filesystem>)
  namespace filesystem = ::std::filesystem;
#else
  namespace filesystem = ::boost::filesystem;
#endif

  filesystem::path prefix_path = prefix;

  // Figure out the directory to use.
  std::string dir;
  if (prefix_path.has_parent_path()) {
    dir = prefix_path.parent();
  } else {
#if __cplusplus >= 201703L // mutable std::string::data()
    dir.resize(MAX_PATH + 1);
    auto dirlen = GetTempPath(dir.size(), dir.data());
    if (dirlen > dir.size()) {
      dir.resize(dirlen + 1);
      dirlen = GetTempPath(dir.size(), dir.data());
    }
    if (dirlen == 0) throw_last_error_();
    assert(dirlen <= dir.size());
    dir.resize(dirlen);
#else
    std::vector<char> dirbuf;
    dirbuf.resize(MAX_PATH + 1);
    auto dirlen = GetTempPath(dirbuf.size(), dirbuf.data());
    if (dirlen > dirbuf.size()) {
      dirbuf.resize(dirlen + 1);
      dirlen = GetTempPath(dirbuf.size(), dirbuf.data());
    }
    if (dirlen == 0) throw_last_error_();
    assert(dirlen <= dir.size());
    dir.assign(std::copy(dirbuf.begin(), dirbuf.begin() + dirlen));
#endif
  }

  // Figure out the temporary file name.
  std::string name;
#if __cplusplus >= 201703L // mutable std::string::data()
  name.resize(MAX_PATH + 1);
  GetTempFileName(dir.c_str(), prefix_path.filename().c_str(), 0, name.data());
  const auto name_end = name.find('\0');
  if (name_end != std::string::npos) name.resize(name_end);
#else
  std::vector<char> namebuf;
  namebuf.resize(MAX_PATH + 1);
  GetTempFileName(dir.c_str(), prefix_path.filename().c_str(), 0, namebuf.data());
  name.assign(namebuf.data()); // zero-terminated string
#endif

  // Create the new file.
  new_fd.handle_ = CreateFile(
      fname.c_str(),
      GENERIC_READ | GENERIC_WRITE,
      0, // No sharing of tmp files.
      nullptr,
      CREATE_NEW,
      FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
      nullptr);
  if (new_fd.handle_ == INVALID_HANDLE_VALUE) throw_last_error_();
  new_fd.mode_ = READ_WRITE;
  return new_fd;
}

void fd::close() {
  if (handle_ != INVALID_HANDLE_VALUE) {
    CloseHandle(handle_);
    handle_ = INVALID_HANDLE_VALUE;
  }
}

void fd::unlink() {
  const auto opt_path = get_path();
  if (!opt_path.has_value()) throw_last_error_(ERROR_INVALID_HANDLE);
  if (!DeleteFile(opt_path.value().c_str())) throw_last_error_();
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

std::optional<std::string> fd::get_path() const {
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

void fd::truncate(size_type sz) {
  FILE_END_OF_FILE_INFO v;
  v.EndOfFile = sz;
  if (!SetFileInformationByHandle(handle_, FileEndOfFileInfo, &v, sizeof(v)))
    throw_last_error_();
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

#include <cstdlib>
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
: handle_(std::exchange(o.handle_, -1)),
  fname_(std::move(o.fname_)),
  mode_(std::move(o.mode_))
{}

fd& fd::operator=(fd&& o) noexcept {
  using std::swap;

  close();
  swap(handle_, o.handle_);
  fname_ = std::move(o.fname_);
  mode_ = std::move(o.mode_);
  return *this;
}

fd::fd(const std::string& fname, open_mode mode)
: handle_(-1),
  fname_(normalize(fname)),
  mode_(mode)
{
  int fl = 0;
#ifdef O_CLOEXEC
  fl |= O_CLOEXEC;
#endif
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

  try {
    struct stat sb;
    auto fstat_rv = ::fstat(handle_, &sb);
    if (fstat_rv != 0) throw_errno_();
    if (!S_ISREG(sb.st_mode)) {
      errno = EFTYPE;
      throw_errno_();
    }
  } catch (...) {
    ::close(handle_);
  }
}

fd::~fd() noexcept {
  close();
}

fd fd::create(const std::string& fname, open_mode mode) {
  fd new_fd;

  int fl = O_CREAT | O_EXCL;
#ifdef O_CLOEXEC
  fl |= O_CLOEXEC;
#endif
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

  new_fd.mode_ = mode;
  new_fd.handle_ = ::open(fname.c_str(), fl, 0666);
  if (new_fd.handle_ == -1) throw_errno_();
  new_fd.fname_ = normalize(fname);
  return new_fd;
}

fd fd::tmpfile(const std::string& prefix) {
  using namespace std::literals;
  static const std::string tmpl_replacement = "XXXXXX"s;

#if  __has_include(<filesystem>)
  namespace filesystem = ::std::filesystem;
#else
  namespace filesystem = ::boost::filesystem;
#endif

  fd new_fd;

  filesystem::path prefix_path = prefix + tmpl_replacement;
  if (prefix_path.has_parent_path()) {
    prefix_path = filesystem::absolute(prefix_path);
  } else {
    prefix_path = filesystem::absolute(
        prefix_path,
        filesystem::temp_directory_path());
  }

#if __cplusplus >= 201703L // std::string::data() is modifiable
  std::string template_name = prefix_path;
#else
  const std::string ppstr = prefix_path.native();
  std::vector<char> template_name(ppstr.begin(), ppstr.end());
  template_name.push_back('\0');
#endif

#ifdef HAS_MKSTEMP
  new_fd.handle_ = mkstemp(template_name.data());
# if __cplusplus >= 201703L // template_name is a string
  new_fd.fname_ = std::move(template_name);
# else // template_name is a vector with a trailing zero-byte ('\0')
  new_fd.fname_ = std::string(template_name.begin(), template_name.end() - 1);
# endif
#else
  new_fd = fd::create(mktemp(template_name.data()));
#endif

  new_fd.mode_ = READ_WRITE;
  assert(new_fd.fname_.substr(new_fd.fname_.length() - 6) != tmpl_replacement);
  ::unlink(new_fd.fname_.c_str()); // Unchecked return
  return new_fd;
}

void fd::close() {
  if (handle_ != -1) {
    ::close(handle_);
    handle_ = -1;
  }
}

void fd::unlink() {
  if (handle_ == -1) {
    errno = EINVAL;
    throw_errno_();
  }
  if (::unlink(fname_.c_str()) != 0) throw_errno_();
}

fd::operator bool() const noexcept {
  return handle_ != -1;
}

auto fd::offset() const -> offset_type {
  const auto off = lseek(handle_, 0, SEEK_CUR);
  if (off == -1) throw_errno_();
  return off;
}

std::optional<std::string> fd::get_path() const {
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

void fd::truncate(size_type sz) {
  if (ftruncate(handle_, sz))
    throw_errno_();
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


namespace monsoon {
namespace io {


std::string fd::normalize(const std::string& fname) {
#if  __has_include(<filesystem>)
  namespace filesystem = ::std::filesystem;
#else
  namespace filesystem = ::boost::filesystem;
#endif

  filesystem::path path = fname;
  return filesystem::canonical(path).native();
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
