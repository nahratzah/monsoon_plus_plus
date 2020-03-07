#include <monsoon/io/aio.h>

#if !defined(WIN32) && __has_include(<aio.h>)
# include <algorithm>
# include <array>
# include <cassert>
# include <cerrno>
# include <cstring>
# include <system_error>
# include <fcntl.h> // For O_SYNC macro.
#else
# include <monsoon/io/rw.h>
#endif

namespace monsoon::io {


#if !defined(WIN32) && __has_include(<aio.h>)

// Work around missing O_DSYNC.
# ifndef O_DSYNC
#   define O_DSYNC O_SYNC
# endif


aio::op_::op_(fd& f) noexcept
: f(f)
{
  reset_();
}

aio::op_::~op_() noexcept {
  if (started_) {
    // Try to cancel.
    ::aio_cancel(f.underlying(), this);

    // We call the normal calls, but since we can't act on errors,
    // we skip checking errors.
    std::array<const ::aiocb*, 1> iovec{{ this }};
    while (::aio_suspend(iovec.data(), iovec.size(), nullptr) && errno == EINTR) {}
    ::aio_error(this);
    ::aio_return(this);
  }
}

bool aio::op_::on_len(std::size_t len) {
  assert(!started_); // Cleared by caller.
  do_on_len(len);
  return started_;
}

void aio::op_::reset_() noexcept {
  ::aiocb& parent = *this;
  std::memset(&parent, 0, sizeof(parent));
  aio_fildes = f.underlying();
}

void aio::op_::enqueue_() {
  std::array<::aiocb*, 1> iovec{{ this }};
  ::lio_listio(LIO_NOWAIT, iovec.data(), iovec.size(), nullptr);
  started_ = true;
}


class aio::read_op_ final
: public op_
{
  public:
  read_op_(fd& f, monsoon::io::fd::offset_type off, void* buf, std::size_t len) noexcept
  : op_(f),
    off_(off),
    buf_(reinterpret_cast<std::uint8_t*>(buf)),
    len_(len)
  {
    aio_lio_opcode = LIO_READ;
    aio_offset = off_;
    aio_buf = buf_;
    aio_nbytes = len_;
  }

  private:
  void do_on_len(std::size_t len) override {
    assert(len > 0);
    assert(len <= len_);

    off_ += len;
    buf_ += len;
    len_ -= len;

    if (len_ != 0) {
      reset_();
      enqueue_();
    }
  }

  void reset_() noexcept {
    this->op_::reset_();
    aio_lio_opcode = LIO_READ;
    aio_offset = off_;
    aio_buf = buf_;
    aio_nbytes = len_;
  }

  monsoon::io::fd::offset_type off_;
  std::uint8_t* buf_;
  std::size_t len_;
};

class aio::write_op_ final
: public op_
{
  public:
  write_op_(fd& f, monsoon::io::fd::offset_type off, const void* buf, std::size_t len) noexcept
  : op_(f),
    off_(off),
    buf_(reinterpret_cast<const std::uint8_t*>(buf)),
    len_(len)
  {
    aio_lio_opcode = LIO_WRITE;
    aio_offset = off_;
    aio_buf = const_cast<std::uint8_t*>(buf_);
    aio_nbytes = len_;
  }

  private:
  void do_on_len(std::size_t len) override {
    assert(len > 0);
    assert(len <= len_);

    off_ += len;
    buf_ += len;
    len_ -= len;

    if (len_ != 0) {
      reset_();
      enqueue_();
    }
  }

  void reset_() noexcept {
    this->op_::reset_();
    aio_lio_opcode = LIO_WRITE;
    aio_offset = off_;
    aio_buf = const_cast<std::uint8_t*>(buf_);
    aio_nbytes = len_;
  }

  monsoon::io::fd::offset_type off_;
  const std::uint8_t* buf_;
  std::size_t len_;
};

class aio::flush_op_ final
: public op_
{
  public:
  flush_op_(fd& f, bool dataonly)
  : op_(f),
    data_only_(dataonly)
  {
    if (::aio_fsync(data_only_ ? O_DSYNC : O_SYNC, this))
      throw std::system_error(errno, std::system_category());
    started_ = true;
  }

  private:
  void do_on_len(std::size_t len) override {
    assert(len == 0);
  }

  bool data_only_;
};


aio::~aio() noexcept {}

void aio::start() {
  if (started_) return;

  std::vector<::aiocb*> iovec;
  iovec.reserve(ops_.size());
  for (const std::unique_ptr<op_>& op : ops_) iovec.push_back(op.get());

  // Start all read/write operations.
  if (::lio_listio(LIO_NOWAIT, iovec.data(), iovec.size(), nullptr)) {
    // Recovery block if an error happened.
    const auto saved_errno = errno;
    switch (saved_errno) {
      case EAGAIN: [[fallthrough]];
      case EINTR: [[fallthrough]];
      case EIO:
        for (const auto& io_ptr : iovec) {
          op_& op = static_cast<op_&>(*io_ptr);

          if (::aio_error(io_ptr) != -1 && errno != EINVAL)
            op.started_ = true; // Will cause the destructor to do the cleanup.
        }
        break;
    }
    throw std::system_error(saved_errno, std::system_category());
  }

  // Mark everything as started.
  for (const std::unique_ptr<op_>& op : ops_) op->started_ = true;
  started_ = true;
}

void aio::join() {
  ops_.reserve(ops_.size() + flush_map_.size());
  std::vector<::aiocb*> iovec;
  iovec.reserve(ops_.size() + flush_map_.size());

  // Enqueue all unblocked flushes.
  auto fm_iter = flush_map_.begin();
  while (fm_iter != flush_map_.end()) {
    if (fm_iter->second.wait_count == 0) {
      ops_.push_back(std::make_unique<flush_op_>(*fm_iter->first, fm_iter->second.dataonly));
      fm_iter = flush_map_.erase(fm_iter);
    } else {
      ++fm_iter;
    }
  }

  // Fill iovec with all operations.
  for (const auto& op : ops_) {
    assert(op->started_);
    iovec.push_back(op.get());
  }

  while (!iovec.empty()) {
    // Wait for any operation to complete.
    if (::aio_suspend(iovec.data(), iovec.size(), nullptr)) {
      if (errno == EINTR) continue; // Interrupted by system call.
      throw std::system_error(errno, std::system_category());
    }

    // Handle responses and figure out what is to be removed.
    const auto erase_end = iovec.end();
    iovec.erase(
        std::remove_if(
            iovec.begin(), iovec.end(),
            [this](auto& io_ptr) {
              op_& op = static_cast<op_&>(*io_ptr);
              assert(op.started_);
              const int e = ::aio_error(io_ptr);

              switch (e) {
                default:
                  ::aio_return(io_ptr); // Call failed, so clear aio_return.
                  op.started_ = false; // We've consumed the aio_return.
                  throw std::system_error(e, std::system_category());
                case -1: // aio_error call failed.
                  throw std::system_error(errno, std::system_category());
                case EINPROGRESS: // Don't discard in-progress calls.
                  return false;
                case 0: { // Handle completion.
                  const auto r = ::aio_return(io_ptr);
                  op.started_ = false; // We've consumed the aio_return.
                  if (r < 0) throw std::system_error(errno, std::system_category());
                  const bool is_restarted = op.on_len(r);
                  if (is_restarted) return false;
                }
              }

              // Item is going to be removed.
              // We need to update the flush map first.
              auto fm_iter = flush_map_.find(&op.f);
              if (fm_iter != flush_map_.end()) {
                if (--fm_iter->second.wait_count == 0) {
                  // Move unblocked flush from flush_map into ops queue.
                  ops_.push_back(std::make_unique<flush_op_>(*fm_iter->first, fm_iter->second.dataonly));
                  flush_map_.erase(fm_iter);
                  io_ptr = ops_.back().get(); // Replace expired aiocb with flush operation.
                  return false;
                }
              }

              // Erase this pointer.
              return true;
            }),
        erase_end);
  }

  ops_.clear();
}

void aio::start_and_join() {
  start();
  join();
}
#endif /* !defined(WIN32) && __has_include(<aio.h>) */


void aio::fd_target::read_at(monsoon::io::fd::offset_type off, void* buf, std::size_t buflen) {
  assert(fd_ != nullptr);
  assert(aio_ != nullptr);

#if !defined(WIN32) && __has_include(<aio.h>)
  const auto fm_iter = aio_->flush_map_.find(fd_);
  aio_->ops_.push_back(std::make_unique<read_op_>(*fd_, off, buf, buflen));
  try {
    if (aio_->started_) aio_->ops_.back()->enqueue_();
  } catch (...) {
    aio_->ops_.pop_back();
    throw;
  }
  if (fm_iter != aio_->flush_map_.end()) ++fm_iter->second.wait_count;
#else
  monsoon::io::read_at(*fd_, off, buf, buflen);
#endif
}

void aio::fd_target::write_at(monsoon::io::fd::offset_type off, const void* buf, std::size_t buflen) {
  assert(fd_ != nullptr);
  assert(aio_ != nullptr);

#if !defined(WIN32) && __has_include(<aio.h>)
  const auto fm_iter = aio_->flush_map_.find(fd_);
  aio_->ops_.push_back(std::make_unique<write_op_>(*fd_, off, buf, buflen));
  try {
    if (aio_->started_) aio_->ops_.back()->enqueue_();
  } catch (...) {
    aio_->ops_.pop_back();
    throw;
  }
  if (fm_iter != aio_->flush_map_.end()) ++fm_iter->second.wait_count;
#else
  monsoon::io::write_at(*fd_, off, buf, buflen);
#endif
}

void aio::fd_target::flush(bool data_only) {
  assert(fd_ != nullptr);
  assert(aio_ != nullptr);

#if !defined(WIN32) && __has_include(<aio.h>)
  auto ins = aio_->flush_map_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(fd_),
      std::forward_as_tuple(0, data_only));

  // Handle case where multiple flush apply.
  if (!std::get<1>(ins) && !data_only)
    std::get<0>(ins)->second.dataonly = false;

  // Handle newly inserted object.
  if (std::get<1>(ins)) {
    // Fix the barrier level.
    std::get<0>(ins)->second.wait_count = std::count_if(
        aio_->ops_.begin(), aio_->ops_.end(),
        [this](const auto& op_ptr) -> bool {
          return &op_ptr->f == fd_;
        });
  }
#else
  auto& setting = aio_->flush_map_[fd_];
  if (!data_only) setting = true;
#endif
}


void aio::const_fd_target::read_at(monsoon::io::fd::offset_type off, void* buf, std::size_t buflen) {
  assert(fd_ != nullptr);
  assert(aio_ != nullptr);

#if !defined(WIN32) && __has_include(<aio.h>)
  const auto fm_iter = aio_->flush_map_.find(const_cast<monsoon::io::fd*>(fd_));
  aio_->ops_.push_back(std::make_unique<read_op_>(const_cast<monsoon::io::fd&>(*fd_), off, buf, buflen));
  try {
    if (aio_->started_) aio_->ops_.back()->enqueue_();
  } catch (...) {
    aio_->ops_.pop_back();
    throw;
  }
  if (fm_iter != aio_->flush_map_.end()) ++fm_iter->second.wait_count;
#else
  monsoon::io::read_at(*fd_, off, buf, buflen);
#endif
}


#if defined(WIN32) || !__has_include(<aio.h>)
void aio::start() {
  std::for_each(
      flush_map_.begin(), flush_map_.end(),
      [](const auto& kv) {
        kv.first->flush(!kv.second);
      });
  flush_map_.clear();
}
#endif


} /* namespace monsoon::io */
