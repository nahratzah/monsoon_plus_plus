#include <monsoon/io/aio.h>

#if !defined(WIN32) && __has_include(<aio.h>)
# include <algorithm>
# include <cerrno>
# include <iterator>
# include <system_error>
# include <fcntl.h> // For O_SYNC macro.
#endif

namespace monsoon::io {


#if !defined(WIN32) && __has_include(<aio.h>)

// Work around missing O_DSYNC.
# ifndef O_DSYNC
#   define O_DSYNC O_SYNC
# endif

auto aio::flush_datum::execute() const -> aiocb* {
  if (::aio_fsync(data_only_ ? O_DSYNC : O_SYNC, ptr_.get()) != 0)
    throw std::system_error(errno, std::system_category());
  return ptr_.get();
}


aio::~aio() noexcept {
  if (!aiocb_vector_.empty()) join();
}

void aio::start() {
  if (!aiocb_vector_.empty()) throw std::logic_error("aio already started");

  try {
    aiocb_vector_ = build_aiocb_vector_();
    if (::lio_listio(LIO_NOWAIT, aiocb_vector_.data(), aiocb_vector_.size(), nullptr) != 0)
      throw std::system_error(errno, std::system_category());

    std::transform(
        flush_vector_.begin(), flush_vector_.end(),
        std::back_inserter(aiocb_vector_),
        [](const flush_datum& f) {
          return f.execute();
        });
  } catch (...) {
    cancel_recover_();
    throw;
  }
}

void aio::join() {
  try {
    if (std::any_of(
        aiocb_vector_.begin(), aiocb_vector_.end(),
        [](const auto ptr) {
          return ptr != nullptr;
        })) {
      const int suspend_rv = ::aio_suspend(aiocb_vector_.data(), aiocb_vector_.size(), nullptr);
      if (suspend_rv == 0) {
        std::size_t index = 0;
        for (auto cb_iter = aiocb_vector_.begin(); cb_iter != aiocb_vector_.end(); ++cb_iter, ++index) {
          auto& cb_ptr = *cb_iter;
          const bool is_flush = index >= datum_vector_.size();

          if (cb_ptr == nullptr) continue;

          const auto e = ::aio_error(cb_ptr);
          assert(e != ECANCELED);
          if (e == EINPROGRESS) {
            continue;
          } else if (e == 0) {
            if (is_flush) {
              cb_ptr = nullptr;
            } else {
              const auto rv = ::aio_return(cb_ptr);
              if (rv == 0)
                throw std::runtime_error("aio operation processed zero bytes");
              assert(rv > 0); // Otherwise aio_error would have told us.
              if (rv >= 0 && cb_ptr->aio_nbytes == std::size_t(rv)) {
                cb_ptr = nullptr;
              } else {
                // Adjust pointers and restart the call.
                cb_ptr->aio_buf = reinterpret_cast<std::uint8_t*>(const_cast<void*>(cb_ptr->aio_buf)) + rv;
                cb_ptr->aio_nbytes -= rv;
                if (::lio_listio(LIO_NOWAIT, &cb_ptr, 1, nullptr) != 0)
                  throw std::system_error(errno, std::system_category());
              }
            }
          } else if (e == -1) {
            throw std::system_error(errno, std::system_category());
          } else {
            throw std::system_error(e, std::system_category());
          }
        }
      } else if (errno != EAGAIN && errno != EINTR) {
        throw std::system_error(errno, std::system_category());
      }
    }
  } catch (...) {
    cancel_recover_();
    throw;
  }
}

void aio::start_and_join() {
  start();
  join();
}

void aio::add_datum(datum&& d) {
  if (!aiocb_vector_.empty())
    throw std::logic_error("can't add operations after starting AIO");
  datum_vector_.emplace_back(std::move(d));
}

void aio::add_datum(flush_datum&& d) {
  if (!aiocb_vector_.empty())
    throw std::logic_error("can't add operations after starting AIO");
  flush_vector_.emplace_back(std::move(d));
}

auto aio::build_aiocb_vector_() const -> std::vector<aiocb*> {
  std::vector<aiocb*> tmp;
  tmp.reserve(datum_vector_.size() + flush_vector_.size());

  std::transform(
      datum_vector_.begin(), datum_vector_.end(),
      std::back_inserter(tmp),
      [](const datum& d) -> aiocb* {
        if (d->aio_nbytes == 0) return nullptr;
        return d.ptr_.get();
      });
  return tmp;
}

void aio::cancel_recover_() noexcept {
  // Anything that's completed, zero it.
  // Anything in progress, try to cancel it.
  for (auto& cb_ptr : aiocb_vector_) {
    if (cb_ptr == nullptr) continue;

    switch (::aio_error(cb_ptr)) {
      default:
        assert(false);
        break;
      case -1:
        if (errno == EINVAL) cb_ptr = nullptr;
        break;
      case 0: // completed
        cb_ptr = nullptr;
        break;
      case EINPROGRESS:
        if (::aio_cancel(cb_ptr->aio_fildes, cb_ptr) != AIO_NOTCANCELED)
          cb_ptr = nullptr;
        break;
      case ECANCELED:
        break;
    }
  }

  // Wait and collect anything remaining.
  while (std::any_of(
      aiocb_vector_.begin(), aiocb_vector_.end(),
      [](const auto ptr) {
        return ptr != nullptr;
      })) {
    if (::aio_suspend(aiocb_vector_.data(), aiocb_vector_.size(), nullptr) == 0) {
      for (auto& cb_ptr : aiocb_vector_) {
        if (cb_ptr == nullptr) continue;

        if (::aio_error(cb_ptr) != EINPROGRESS) cb_ptr = nullptr;
      }
    } else {
      switch (errno) {
        default:
          // In debug mode, we panic, but in non-debug, we silently ignore the error.
          assert(false);
          break;
        case EAGAIN:
          break;
        case EINTR:
          break;
      }
    }
  }

  aiocb_vector_.clear();
}
#endif /* !defined(WIN32) && __has_include(<aio.h>) */


void aio::fd_target::read_at(monsoon::io::fd::offset_type off, void* buf, std::size_t buflen) {
  assert(fd_ != nullptr);
  assert(aio_ != nullptr);

#if !defined(WIN32) && __has_include(<aio.h>)
  aio::datum d;
  d->aio_fildes = fd_->underlying();
  d->aio_offset = off;
  d->aio_buf = buf;
  d->aio_nbytes = buflen;
  d->aio_lio_opcode = LIO_READ;

  aio_->add_datum(std::move(d));
#else
  while (buflen > 0) {
    const auto rlen = fd_->read_at(off, buf, buflen);
    if (rlen == 0) throw std::runtime_error("aio operation processed zero bytes");
    buf = reinterpret_cast<std::uint8_t*>(buf) + rlen;
    buflen -= rlen;
    off += rlen;
  }
#endif
}

void aio::fd_target::write_at(monsoon::io::fd::offset_type off, const void* buf, std::size_t buflen) {
  assert(fd_ != nullptr);
  assert(aio_ != nullptr);

#if !defined(WIN32) && __has_include(<aio.h>)
  aio::datum d;
  d->aio_fildes = fd_->underlying();
  d->aio_offset = off;
  d->aio_buf = const_cast<void*>(buf);
  d->aio_nbytes = buflen;
  d->aio_lio_opcode = LIO_WRITE;

  aio_->add_datum(std::move(d));
#else
  while (buflen > 0) {
    const auto wlen = fd_->write_at(off, buf, buflen);
    if (wlen == 0) throw std::runtime_error("aio operation processed zero bytes");
    buf = reinterpret_cast<const std::uint8_t*>(buf) + wlen;
    buflen -= wlen;
    off += wlen;
  }
#endif
}

void aio::fd_target::flush(bool data_only) {
  assert(fd_ != nullptr);
  assert(aio_ != nullptr);

#if !defined(WIN32) && __has_include(<aio.h>)
  aio_->add_datum(aio::flush_datum(*fd_, data_only));
#else
  auto& setting = aio_->flush_map_[fd_];
  if (!data_only) setting = true;
#endif
}


void aio::const_fd_target::read_at(monsoon::io::fd::offset_type off, void* buf, std::size_t buflen) {
  assert(fd_ != nullptr);
  assert(aio_ != nullptr);

#if !defined(WIN32) && __has_include(<aio.h>)
  aio::datum d;
  d->aio_fildes = fd_->underlying();
  d->aio_offset = off;
  d->aio_buf = buf;
  d->aio_nbytes = buflen;
  d->aio_lio_opcode = LIO_READ;

  aio_->add_datum(std::move(d));
#else
  while (buflen > 0) {
    const auto rlen = fd_->read_at(off, buf, buflen);
    if (rlen == 0) throw std::runtime_error("aio operation processed zero bytes");
    buf = reinterpret_cast<std::uint8_t*>(buf) + rlen;
    buflen -= rlen;
    off += rlen;
  }
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
