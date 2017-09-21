#ifndef MONSOON_XDR_STREAM_INL_H
#define MONSOON_XDR_STREAM_INL_H

#include <utility>

namespace monsoon {
namespace xdr {


template<typename Reader>
xdr_stream_reader<Reader>::xdr_stream_reader(Reader&& r)
noexcept(std::is_nothrow_move_constructible<Reader>())
: r_(std::move(r))
{}

template<typename Reader>
xdr_stream_reader<Reader>::xdr_stream_reader(xdr_stream_reader&& o)
noexcept(std::is_nothrow_move_constructible<Reader>())
: xdr_istream(std::move(o)),
  r_(std::move(o.r_))
{}

template<typename Reader>
auto xdr_stream_reader<Reader>::operator=(xdr_stream_reader&& o)
noexcept(std::is_nothrow_move_constructible<Reader>())
-> xdr_stream_reader& {
  static_cast<xdr_istream&>(*this) = std::move(o);
  r_ = std::move(o.r_);
  return *this;
}

template<typename Reader>
void xdr_stream_reader<Reader>::get_raw_bytes(void* buf, std::size_t len) {
  while (len > 0) {
    const std::size_t rlen = r_.read(buf, len);
    if (rlen == 0) throw xdr_exception();

    len -= rlen;
    buf = reinterpret_cast<std::uint8_t*>(buf) + rlen;
  }
}


template<typename Writer>
xdr_stream_writer<Writer>::xdr_stream_writer(Writer&& w)
noexcept(std::is_nothrow_move_constructible<Writer>())
: w_(std::move(w))
{}

template<typename Writer>
xdr_stream_writer<Writer>::xdr_stream_writer(xdr_stream_writer&& o)
noexcept(std::is_nothrow_move_constructible<Writer>())
: xdr_istream(std::move(o)),
  w_(std::move(o.w_))
{}

template<typename Writer>
auto xdr_stream_writer<Writer>::operator=(xdr_stream_writer&& o)
noexcept(std::is_nothrow_move_constructible<Writer>())
-> xdr_stream_writer& {
  static_cast<xdr_ostream&>(*this) = std::move(o);
  w_ = std::move(o.w_);
  return *this;
}

template<typename Writer>
void xdr_stream_writer<Writer>::put_raw_bytes(const void* buf,
    std::size_t len) {
  while (len > 0) {
    const std::size_t wlen = w_.write(buf, len);
    if (wlen == 0) throw xdr_exception();

    len -= wlen;
    buf = reinterpret_cast<const std::uint8_t*>(buf) + wlen;
  }
}


}} /* namespace monsoon::xdr */

#endif /* MONSOON_XDR_STREAM_INL_H */
