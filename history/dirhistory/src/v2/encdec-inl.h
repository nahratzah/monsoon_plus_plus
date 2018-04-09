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


template<typename T, typename H>
dictionary<T, H>::dictionary()
: decode_map_(),
  encode_map_(create_encode_map_future_(*this))
{}

template<typename T, typename H>
dictionary<T, H>::dictionary(const dictionary& other)
: decode_map_(other.decode_map_),
  encode_map_(create_encode_map_future_(*this)),
  update_start_(other.update_start_)
{}

template<typename T, typename H>
auto dictionary<T, H>::operator=(const dictionary& other)
-> dictionary& {
  decode_map_ = other.decode_map_;
  encode_map_ = create_encode_map_future_(*this);
  update_start_ = other.update_start_;
  return *this;
}

template<typename T, typename H>
std::uint32_t dictionary<T, H>::encode(view_type v) {
  if constexpr(std::is_same_v<std::string, T>) {
    auto ptr = encode_map_.get().find(std::string(v.begin(), v.end()));
    if (ptr != encode_map_.get().end()) return ptr->second;
  } else {
    auto ptr = encode_map_.get().find(v);
    if (ptr != encode_map_.get().end()) return ptr->second;
  }

  if (decode_map_.size() > 0xffffffffu)
    throw xdr::xdr_exception("dictionary too large");
  const std::uint32_t new_idx = decode_map_.size();

  if constexpr(std::is_same_v<std::string, T>)
    decode_map_.push_back(std::string(v.begin(), v.end()));
  else
    decode_map_.push_back(v);
  try {
    if constexpr(std::is_same_v<std::string, T>)
      encode_map_.get().emplace(std::string(v.begin(), v.end()), new_idx);
    else
      encode_map_.get().emplace(v, new_idx);
  } catch (...) {
    decode_map_.pop_back();
    throw;
  }
  return new_idx;
}

template<typename T, typename H>
auto dictionary<T, H>::decode(std::uint32_t idx) const -> view_type {
  if (idx >= decode_map_.size())
    throw xdr::xdr_exception("invalid dictionary index");
  return decode_map_[idx];
}

template<typename T, typename H>
template<typename SerFn>
void dictionary<T, H>::decode_update(xdr::xdr_istream& in, SerFn fn) {
  if (encode_map_.wait_for(std::chrono::seconds(0)) != std::future_status::deferred)
    encode_map_ = create_encode_map_future_(*this);

  const std::uint32_t offset = in.get_uint32();
  if (offset != decode_map_.size())
    throw xdr::xdr_exception("dictionary updates must be contiguous");

  in.get_collection(fn, decode_map_);
  if (decode_map_.size() > 0xffffffffu)
    throw xdr::xdr_exception("dictionary too large");
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

template<typename T, typename H>
auto dictionary<T, H>::create_encode_map_future_(const dictionary& self)
-> std::future<std::unordered_map<T, std::uint32_t, H>> {
  // We may not capture self, because of the move constructor, but
  // capturing the iterators is safe, since move won't invalidate them.
  auto b = self.decode_map_.begin();
  auto e = self.decode_map_.end();

  return std::async(
      std::launch::deferred,
      [b, e]() mutable {
        std::unordered_map<T, std::uint32_t, H> result;
        typename std::vector<T>::size_type idx = 0;
        while (b != e)
          result.emplace(*b++, idx++);
        return result;
      });
}


template<typename H>
dictionary<std::vector<std::string>, H>::dictionary()
: decode_map_(),
  encode_map_(create_encode_map_future_(*this))
{}

template<typename H>
dictionary<std::vector<std::string>, H>::dictionary(const dictionary& other)
: decode_map_(other.decode_map_),
  encode_map_(create_encode_map_future_(*this)),
  update_start_(other.update_start_)
{}

template<typename H>
auto dictionary<std::vector<std::string>, H>::operator=(const dictionary& other)
-> dictionary& {
  decode_map_ = other.decode_map_;
  encode_map_ = create_encode_map_future_(*this);
  update_start_ = other.update_start_;
  return *this;
}

template<typename H>
template<typename T, typename Alloc>
std::uint32_t dictionary<std::vector<std::string>, H>::encode(const std::vector<T, Alloc>& v_tmpl) {
  std::vector<std::string> v;
  if constexpr(std::is_same_v<std::vector<std::string>, std::vector<T, Alloc>>) {
    v = v_tmpl;
  } else {
    std::for_each(v_tmpl.begin(), v_tmpl.end(),
        [&v](std::string_view s) {
          v.emplace_back(s.begin(), s.end());
        });
  }

  auto ptr = encode_map_.get().find(v);
  if (ptr != encode_map_.get().end()) return ptr->second;

  if (decode_map_.size() > 0xffffffffu)
    throw xdr::xdr_exception("dictionary too large");
  const std::uint32_t new_idx = decode_map_.size();

  decode_map_.push_back(v);
  try {
    encode_map_.get().emplace(std::move(v), new_idx);
  } catch (...) {
    decode_map_.pop_back();
    throw;
  }
  return new_idx;
}

template<typename H>
auto dictionary<std::vector<std::string>, H>::decode(std::uint32_t idx) const -> const std::vector<std::string>& {
  if (idx >= decode_map_.size())
    throw xdr::xdr_exception("invalid dictionary index");
  return decode_map_[idx];
}

template<typename H>
template<typename SerFn>
void dictionary<std::vector<std::string>, H>::decode_update(xdr::xdr_istream& in, SerFn fn) {
  if (encode_map_.wait_for(std::chrono::seconds(0)) != std::future_status::deferred)
    encode_map_ = create_encode_map_future_(*this);

  const std::uint32_t offset = in.get_uint32();
  if (offset != decode_map_.size())
    throw xdr::xdr_exception("dictionary updates must be contiguous");

  in.get_collection(fn, decode_map_);
  if (decode_map_.size() > 0xffffffffu)
    throw xdr::xdr_exception("dictionary too large");
  update_start_ = decode_map_.size();
}

template<typename H>
template<typename SerFn>
void dictionary<std::vector<std::string>, H>::encode_update(xdr::xdr_ostream& out, SerFn fn) {
  out.put_uint32(update_start_);
  out.put_collection(
      fn,
      decode_map_.begin() + update_start_,
      decode_map_.end());
  update_start_ = decode_map_.size();
}

template<typename H>
bool dictionary<std::vector<std::string>, H>::update_pending() const noexcept {
  return update_start_ < decode_map_.size();
}

template<typename H>
auto dictionary<std::vector<std::string>, H>::create_encode_map_future_(const dictionary& self)
-> std::future<std::unordered_map<std::vector<std::string>, std::uint32_t, H>> {
  // We may not capture self, because of the move constructor, but
  // capturing the iterators is safe, since move won't invalidate them.
  auto b = self.decode_map_.begin();
  auto e = self.decode_map_.end();

  return std::async(
      std::launch::deferred,
      [b, e]() mutable {
        std::unordered_map<std::vector<std::string>, std::uint32_t, H> result;
        std::vector<std::vector<std::string>>::size_type idx = 0;
        while (b != e)
          result.emplace(*b++, idx++);
        return result;
      });
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
