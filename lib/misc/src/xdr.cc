#include <monsoon/xdr/xdr.h>

namespace monsoon {
namespace xdr {


xdr_istream::~xdr_istream() noexcept {}

xdr_ostream::~xdr_ostream() noexcept {}

xdr_exception::xdr_exception() {}

xdr_exception::~xdr_exception() {}

const char* xdr_exception::what() const noexcept {
  return (what_ == nullptr ? "monsoon::xdr::xdr_exception" : what_);
}

xdr_stream_end::~xdr_stream_end() {}


}} /* namespace monsoon::xdr */
