#include <monsoon/xdr/xdr.h>
#include <algorithm>

namespace monsoon {
namespace xdr {


const std::size_t xdr_istream::BUFFER_SIZE = 65536u;

xdr_istream::xdr_istream(xdr_istream&& o) noexcept
: buffer_(std::move(o.buffer_)),
  buffer_off_(std::exchange(o.buffer_off_, 0u))
{}

xdr_istream& xdr_istream::operator=(xdr_istream&& o) noexcept {
  buffer_ = std::move(o.buffer_);
  buffer_off_ = std::exchange(o.buffer_off_, 0u);
  return *this;
}

xdr_istream::~xdr_istream() noexcept {}

void xdr_istream::get_raw_bytes_(void* dst_ptr, std::size_t len) {
  std::uint8_t* dst = reinterpret_cast<std::uint8_t*>(dst_ptr);

  while (len > 0u) {
    if (buffer_off_ < buffer_.size()) {
      const std::size_t rlen = std::min(buffer_.size() - buffer_off_, len);
      std::copy_n(buffer_.data() + buffer_off_, rlen, dst);
      len -= rlen;
      dst += rlen;
      buffer_off_ += rlen;
    }

    if (len > 0u && buffer_off_ == buffer_.size()) {
      if (len > BUFFER_SIZE) { // Pass-through read.
        const std::size_t rlen = get_raw_bytes(dst, len);
        assert(rlen <= len);
        len -= rlen;
        dst += rlen;
        if (rlen == 0u && at_end_()) throw xdr_stream_end();
      } else { // Fill buffer.
        buffer_off_ = 0u;
        buffer_.resize(BUFFER_SIZE);
        const std::size_t rlen = get_raw_bytes(buffer_.data(), buffer_.size());
        assert(rlen <= buffer.size());
        buffer_.resize(rlen);
        if (rlen == 0u && at_end_()) throw xdr_stream_end();
      }
    }
  }
}

bool xdr_istream::at_end() const {
  return buffer_off_ == buffer_.size() && at_end_();
}


xdr_ostream::~xdr_ostream() noexcept {}


xdr_exception::xdr_exception() {}

xdr_exception::~xdr_exception() {}

const char* xdr_exception::what() const noexcept {
  return (what_ == nullptr ? "monsoon::xdr::xdr_exception" : what_);
}


xdr_stream_end::xdr_stream_end() noexcept
: xdr_exception("monsoon::xdr::xdr_stream_end")
{}

xdr_stream_end::~xdr_stream_end() {}


}} /* namespace monsoon::xdr */
