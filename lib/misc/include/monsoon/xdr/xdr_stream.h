#ifndef MONSOON_XDR_STREAM_H
#define MONSOON_XDR_STREAM_H

#include <type_traits>
#include <monsoon/xdr/xdr.h>

namespace monsoon {
namespace xdr {


template<typename Reader>
class xdr_stream_reader
: public xdr_istream
{
 public:
  xdr_stream_reader() = default;

  xdr_stream_reader(Reader&&)
      noexcept(std::is_nothrow_move_constructible<Reader>());

  xdr_stream_reader(xdr_stream_reader&& o)
      noexcept(std::is_nothrow_move_constructible<Reader>());

  xdr_stream_reader& operator=(xdr_stream_reader&& o)
      noexcept(std::is_nothrow_move_constructible<Reader>());

  void close() override;

  Reader& underlying_stream() noexcept {
    return r_;
  }

  const Reader& underlying_stream() const noexcept {
    return r_;
  }

 private:
  bool at_end_() const override;
  std::size_t get_raw_bytes(void*, std::size_t) override;

  Reader r_;
};


template<typename Writer>
class xdr_stream_writer
: public xdr_ostream
{
 public:
  xdr_stream_writer() = default;

  xdr_stream_writer(Writer&&)
      noexcept(std::is_nothrow_move_constructible<Writer>());

  xdr_stream_writer(xdr_stream_writer&& o)
      noexcept(std::is_nothrow_move_constructible<Writer>());

  xdr_stream_writer& operator=(xdr_stream_writer&& o)
      noexcept(std::is_nothrow_move_constructible<Writer>());

  void close() override;

  Writer& underlying_stream() noexcept {
    return w_;
  }

  const Writer& underlying_stream() const noexcept {
    return w_;
  }

 private:
  void put_raw_bytes(const void*, std::size_t) override;

  Writer w_;
};


}} /* namespace monsoon::xdr */

#include "xdr_stream-inl.h"

#endif /* MONSOON_XDR_STREAM_H */
