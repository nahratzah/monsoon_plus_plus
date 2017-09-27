#ifndef V2_ENCDEC_INL_H
#define V2_ENCDEC_INL_H

#include <algorithm>
#include <utility>
#include <monsoon/history/dir/hdir_exception.h>

namespace monsoon {
namespace history {
namespace v2 {


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


template<typename T, typename H>
std::uint32_t dictionary<T, H>::encode(const T& v) {
  auto ptr = encode_map_.find(v);
  if (ptr != encode_map_.end()) return ptr->second;

  if (decode_map_.size() > 0xffffffffu)
    throw xdr::xdr_exception("dictionary too large");
  const std::uint32_t new_idx = decode_map_.size();

  decode_map_.push_back(v);
  try {
    encode_map_.emplace(v, new_idx);
  } catch (...) {
    decode_map_.pop_back();
    throw;
  }
  return new_idx;
}

template<typename T, typename H>
const T& dictionary<T, H>::decode(std::uint32_t idx) const {
  if (idx >= decode_map_.size())
    throw xdr::xdr_exception("invalid dictionary index");
  return decode_map_[idx];
}

template<typename T, typename H>
template<typename SerFn>
void dictionary<T, H>::decode_update(xdr::xdr_istream& in, SerFn fn) {
  const std::uint32_t offset = in.get_uint32();
  if (offset != decode_map_.size())
    throw xdr::xdr_exception("dictionary updates must be contiguous");

  in.accept_collection(
      fn,
      [this, &offset](const T& v) {
        auto idx = decode_map_.size();
        if (idx > 0xffffffffu)
          throw xdr::xdr_exception("dictionary too large");

        decode_map_.push_back(v);
        try {
          encode_map_.emplace(v, offset);
        } catch (...) {
          decode_map_.pop_back();
          throw;
        }
      });
  update_start_ = decode_map_.size();
}

template<typename T, typename H>
template<typename SerFn>
void dictionary<T, H>::encode_update(xdr::xdr_ostream& out, SerFn fn) {
  out.put_uint32(update_start_);
  out.put_collection(
      fn,
      decode_map_.begin() + update_start_,
      decode_map_.end());
  update_start_ = decode_map_.size();
}

template<typename T, typename H>
bool dictionary<T, H>::update_pending() const noexcept {
  return update_start_ < decode_map_.size();
}


template<typename Alloc1, typename Alloc2, typename Char, typename CharT>
std::size_t dictionary_delta::strvector_hasher::operator()(
    const std::vector<std::basic_string<Char, CharT, Alloc2>, Alloc1>& sv)
const noexcept {
  std::hash<std::basic_string<Char, CharT, Alloc2>> inner;
  std::size_t hval = 19;
  std::for_each(sv.begin(), sv.end(),
      [&](const std::basic_string<Char, CharT, Alloc2>& s) {
        hval = 53 * hval + inner(s);
      });
  return hval;
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
