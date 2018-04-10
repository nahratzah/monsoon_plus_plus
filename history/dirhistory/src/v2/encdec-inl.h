#ifndef V2_ENCDEC_INL_H
#define V2_ENCDEC_INL_H

#include <algorithm>
#include <utility>
#include <monsoon/history/dir/hdir_exception.h>

namespace monsoon {
namespace history {
namespace v2 {


inline encdec_writer::encdec_writer(const encdec_ctx& ctx,
    io::fd::offset_type off)
: off_(off),
  ctx_(ctx)
{}

inline auto encdec_writer::begin(bool compress) -> xdr_writer {
  return xdr_writer(*this, compress);
}


inline encdec_writer::xdr_writer::xdr_writer(xdr_writer&& o) noexcept
: buffer_(std::move(o.buffer_)),
  ecw_(std::exchange(o.ecw_, nullptr)),
  compress_(o.compress_),
  ptr_(o.ptr_),
  closed_(o.closed_)
{}

inline auto encdec_writer::xdr_writer::operator=(xdr_writer&& o) noexcept
-> xdr_writer& {
  buffer_ = std::move(o.buffer_);
  ecw_ = std::exchange(o.ecw_, nullptr);
  compress_ = o.compress_;
  ptr_ = o.ptr_;
  closed_ = o.closed_;
  return *this;
}

inline encdec_writer::xdr_writer::xdr_writer(encdec_writer& ecw, bool compress)
  noexcept
: ecw_(&ecw),
  compress_(compress)
{}


inline file_segment_ptr::file_segment_ptr(offset_type off, size_type len)
    noexcept
: off_(off),
  len_(len)
{}


template<typename T>
file_segment<T>::file_segment() noexcept
{}

template<typename T>
file_segment<T>::file_segment(file_segment&& o) noexcept
: ptr_(std::move(o.ptr_)),
  ctx_(std::move(o.ctx_)),
  decoder_(std::move(o.decoder_)),
  enable_compression_(o.enable_compression_)
{}

template<typename T>
auto file_segment<T>::operator=(file_segment&& o) noexcept
-> file_segment& {
  ptr_ = std::move(o.ptr_);
  ctx_ = std::move(o.ctx_);
  decoder_ = std::move(o.decoder_);
  enable_compression_ = o.enable_compression_;
  decode_result_ = std::weak_ptr<const T>();
  return *this;
}

template<typename T>
file_segment<T>::file_segment(const encdec_ctx& ctx, file_segment_ptr ptr,
    std::function<std::shared_ptr<T> (xdr::xdr_istream&)>&& decoder, bool enable_compression)
  noexcept
: ptr_(std::move(ptr)),
  ctx_(ctx),
  decoder_(std::move(decoder)),
  enable_compression_(enable_compression)
{}

template<typename T>
std::shared_ptr<const T> file_segment<T>::get() const {
  std::lock_guard<std::mutex> lck{ lck_ };

  std::shared_ptr<const T> result = decode_result_.lock();
  if (result == nullptr) {
    auto xdr = ctx_.new_reader(ptr_, enable_compression_);
    result = decoder_(xdr);
    if (!xdr.at_end()) throw dirhistory_exception("xdr data remaining");
    xdr.close();
    decode_result_ = result;
  }
  return result;
}

template<typename T>
void file_segment<T>::update_addr(file_segment_ptr ptr) noexcept {
  std::lock_guard<std::mutex> lck{ lck_ };

  ptr_ = ptr;
  decode_result_ = std::weak_ptr<const T>(); // Invalidate cache
}


inline tsdata_list::tsdata_list(
    const encdec_ctx& ctx,
    time_point ts,
    std::optional<file_segment_ptr> pred,
    std::optional<file_segment_ptr> dd,
    file_segment_ptr records,
    std::uint32_t reserved) noexcept
: ts_(std::move(ts)),
  pred_(std::move(pred)),
  dd_(std::move(dd)),
  records_(std::move(records)),
  reserved_(std::move(reserved)),
  ctx_(ctx)
{}


}}} /* namespace monsoon::history::v2 */

#endif /* V2_ENCDEC_INL_H */
