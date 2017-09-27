#include <monsoon/io/ptr_stream.h>
#include <utility>
#include <stdexcept>

namespace monsoon {
namespace io {


ptr_stream_reader::ptr_stream_reader(ptr_stream_reader&& o) noexcept
: nested_(std::move(o.nested_))
{}

auto ptr_stream_reader::operator=(ptr_stream_reader&& o) noexcept
-> ptr_stream_reader& {
  nested_ = std::move(o.nested_);
  return *this;
}

ptr_stream_reader::ptr_stream_reader(std::unique_ptr<stream_reader>&& nested)
  noexcept
: nested_(std::move(nested))
{}

ptr_stream_reader::~ptr_stream_reader() noexcept {}

std::size_t ptr_stream_reader::read(void* buf, std::size_t len) {
  if (nested_ == nullptr) throw std::logic_error("nullptr reader");
  return nested_->read(buf, len);
}

void ptr_stream_reader::close() {
  if (nested_ == nullptr) throw std::logic_error("nullptr reader");
  nested_->close();
}

bool ptr_stream_reader::at_end() const {
  if (nested_ == nullptr) throw std::logic_error("nullptr reader");
  return nested_->at_end();
}


ptr_stream_writer::ptr_stream_writer(ptr_stream_writer&& o) noexcept
: nested_(std::move(o.nested_))
{}

auto ptr_stream_writer::operator=(ptr_stream_writer&& o) noexcept
-> ptr_stream_writer& {
  nested_ = std::move(o.nested_);
  return *this;
}

ptr_stream_writer::ptr_stream_writer(std::unique_ptr<stream_writer>&& nested)
  noexcept
: nested_(std::move(nested))
{}

ptr_stream_writer::~ptr_stream_writer() noexcept {}

std::size_t ptr_stream_writer::write(const void* buf, std::size_t len) {
  if (nested_ == nullptr) throw std::logic_error("nullptr reader");
  return nested_->write(buf, len);
}

void ptr_stream_writer::close() {
  if (nested_ == nullptr) throw std::logic_error("nullptr reader");
  nested_->close();
}


}} /* namespace monsoon::io */
