#include <monsoon/io/positional_stream.h>

namespace monsoon {
namespace io {


positional_reader::~positional_reader() noexcept {}

std::size_t positional_reader::read(void* buf, std::size_t len) {
  if (fd_ == nullptr)
    throw std::logic_error("cannot read without file descriptor");

  const std::size_t rlen = fd_->read_at(off_, buf, len);
  off_ += rlen;
  return rlen;
}

bool positional_reader::at_end() const {
  if (fd_ == nullptr)
    throw std::logic_error("cannot write without file descriptor");

  return off_ == fd_->size();
}

void positional_reader::close() {
  if (fd_ == nullptr)
    throw std::logic_error("cannot write without file descriptor");

  fd_ = nullptr;
}


positional_writer::~positional_writer() noexcept {}

std::size_t positional_writer::write(const void* buf, std::size_t len) {
  if (fd_ == nullptr)
    throw std::logic_error("cannot write without file descriptor");

  const std::size_t rlen = fd_->write_at(off_, buf, len);
  off_ += rlen;
  return rlen;
}

void positional_writer::close() {
  if (fd_ == nullptr)
    throw std::logic_error("cannot write without file descriptor");

  fd_ = nullptr;
}


}} /* namespace monsoon::io */
