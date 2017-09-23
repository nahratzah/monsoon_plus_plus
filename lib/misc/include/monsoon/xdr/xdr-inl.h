#ifndef MONSOON_XDR_XDR_INL_H
#define MONSOON_XDR_XDR_INL_H

#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>
#include <limits>
#include <utility>
#include <boost/endian/conversion.hpp>

namespace monsoon {
namespace xdr {


inline void xdr_istream::get_void() {
  return;
}

inline bool xdr_istream::get_bool() {
  switch (get_uint32()) {
    default:
      throw xdr_exception();
    case 0u:
      return false;
    case 1u:
      return true;
  }
}

inline std::int8_t xdr_istream::get_int8() {
  const std::int32_t v = get_int32();
  if (v < -0x80 || v > 0x7f)
    throw xdr_exception();
  return static_cast<std::int8_t>(v);
}

inline std::uint8_t xdr_istream::get_uint8() {
  const std::uint32_t v = get_uint32();
  if (v > 0xffu)
    throw xdr_exception();
  return static_cast<std::uint8_t>(v);
}

inline std::int16_t xdr_istream::get_int16() {
  const std::int32_t v = get_int32();
  if (v < -0x8000 || v > 0x7fff)
    throw xdr_exception();
  return static_cast<std::int16_t>(v);
}

inline std::uint16_t xdr_istream::get_uint16() {
  const std::uint32_t v = get_uint32();
  if (v > 0xffffu)
    throw xdr_exception();
  return static_cast<std::uint16_t>(v);
}

inline std::int32_t xdr_istream::get_int32() {
  const std::uint32_t i = get_uint32();
  if (i < 0x80000000u)
    return static_cast<std::int32_t>(i);
  else
    return static_cast<std::int32_t>(i - 0x80000000u) - 0x80000000;
}

inline std::uint32_t xdr_istream::get_uint32() {
  std::uint32_t i;
  get_raw_bytes(&i, sizeof(i));
  return boost::endian::big_to_native(i);
}

inline std::int64_t xdr_istream::get_int64() {
  const std::uint64_t i = get_uint64();
  if (i < 0x8000000000000000ul) {
    return static_cast<std::int64_t>(i);
  } else {
    return static_cast<std::int64_t>(i - 0x8000000000000000ul) -
        0x8000000000000000l;
  }
}

inline std::uint64_t xdr_istream::get_uint64() {
  std::uint64_t i;
  get_raw_bytes(&i, sizeof(i));
  return boost::endian::big_to_native(i);
}

inline float xdr_istream::get_flt32() {
  static_assert(std::numeric_limits<float>::is_iec559,
      "require IEEE 754 layout.");
  static_assert(sizeof(float) == sizeof(std::uint32_t),
      "expecting uint32 and float to have same size");

  std::uint32_t tmp = get_uint32();
  float f;
  std::memcpy(&f, &tmp, sizeof(f));
  return f;
}

inline double xdr_istream::get_flt64() {
  static_assert(std::numeric_limits<double>::is_iec559,
      "require IEEE 754 layout.");
  static_assert(sizeof(double) == sizeof(std::uint64_t),
      "expecting uint32 and float to have same size");

  std::uint64_t tmp = get_uint64();
  double f;
  std::memcpy(&f, &tmp, sizeof(f));
  return f;
}

template<typename Alloc>
inline auto xdr_istream::get_string(const Alloc& alloc)
-> std::basic_string<char, std::char_traits<char>, Alloc> {
  std::basic_string<char, std::char_traits<char>, Alloc> result =
      std::basic_string<char, std::char_traits<char>, Alloc>(alloc);

  const std::uint32_t len = get_uint32();
  if (len == 0) return result;
  const std::size_t rounded_len = // len, rounded up to a multiple of 4
      ((static_cast<std::size_t>(len) + 3u) / 4u) * 4u;

#if __cplusplus >= 201703L
  result.resize(rounded_len);
  get_raw_bytes(result.data(), rounded_len);
  // Verify padding of zeroes.
  if (std::any_of(result.begin() + len, result.end(),
          [](char c) { return c != '\0'; }))
    throw xdr_exception();
  if (len != rounded_len) result.resize(len); // Remove excess characters.
#else // std::basic_string::data() is not (safely) modifiable before C++17.
  result.reserve(len);
  auto tmp = std::vector<char, Alloc>(rounded_len, alloc);
  get_raw_bytes(tmp.data(), rounded_len);
  // Verify padding of zeroes.
  if (std::any_of(tmp.begin() + len, tmp.end(),
          [](char c) { return c != '\0'; }))
    throw xdr_exception();
  result.assign(tmp.begin(), tmp.begin() + len);
#endif

  return result;
}

template<typename Alloc>
inline auto xdr_istream::get_opaque_n(std::size_t len, const Alloc& alloc)
-> std::vector<std::uint8_t, Alloc> {
  std::vector<std::uint8_t, Alloc> result =
      std::vector<std::uint8_t, Alloc>(alloc);

  if (len == 0) return result;
  const std::size_t rounded_len = // len, rounded up to a multiple of 4
      ((static_cast<std::size_t>(len) + 3u) / 4u) * 4u;

  result.resize(rounded_len);
  get_raw_bytes(result.data(), rounded_len);
  // Verify padding of zeroes.
  if (std::any_of(result.begin() + len, result.end(),
          [](std::uint8_t c) { return c != 0u; }))
    throw xdr_exception();
  if (len != rounded_len) result.resize(len); // Remove excess characters.
  return result;
}

template<typename Alloc>
inline auto xdr_istream::get_opaque(const Alloc& alloc)
-> std::vector<std::uint8_t, Alloc> {
  return get_opaque_n(get_uint32(), alloc);
}

template<std::size_t Len>
inline auto xdr_istream::get_array(std::array<std::uint8_t, Len>& arr)
-> std::enable_if_t<Len % 4u == 0u, std::array<std::uint8_t, Len>&> {
  get_raw_bytes(arr.data(), arr.size());
  return arr;
}

template<std::size_t Len>
inline auto xdr_istream::get_array(std::array<std::uint8_t, Len>& arr)
-> std::enable_if_t<Len % 4u != 0u, std::array<std::uint8_t, Len>&> {
  get_raw_bytes(arr.data(), arr.size());

  std::array<std::uint8_t, 4u - Len % 4u> padding;
  get_raw_bytes(padding.data(), padding.size());
  if (std::any_of(padding.begin(), padding.end(),
          [](std::uint8_t c) { return c != 0u; }))
    throw xdr_exception();
  return arr;
}

template<std::size_t Len>
inline auto xdr_istream::get_array(std::array<std::uint8_t, Len>&& arr)
-> std::array<std::uint8_t, Len>&& {
  get_array(arr);
  return std::move(arr);
}

template<std::size_t Len>
inline auto xdr_istream::get_array() -> std::array<std::uint8_t, Len> {
  return get_array(std::array<std::uint8_t, Len>());
}

template<typename C, typename SerFn>
inline auto xdr_istream::get_collection_n(std::size_t len, SerFn fn, C&& c)
-> C&& {
  std::generate_n(std::inserter(c, c.end()), len,
      [this, &fn]() { return fn(*this); });
  return std::forward<C>(c);
}

template<typename C, typename SerFn>
inline auto xdr_istream::get_collection_n(std::size_t len, SerFn fn, C& c)
-> C& {
  std::generate_n(std::inserter(c, c.end()), len,
      [this, &fn]() { return fn(*this); });
  return c;
}

template<typename C, typename SerFn>
inline auto xdr_istream::get_collection(SerFn fn, C&& c) -> C&& {
  return get_collection_n(get_uint32(), std::move(fn), std::forward<C>(c));
}

template<typename C, typename SerFn>
inline auto xdr_istream::get_collection(SerFn fn, C& c) -> C& {
  return get_collection_n(get_uint32(), std::move(fn), std::forward<C>(c));
}

template<typename SerFn, typename Acceptor>
inline void xdr_istream::accept_collection_n(std::size_t len, SerFn fn,
    Acceptor acceptor) {
  for (std::size_t i = 0; i < len; ++i)
    acceptor(fn(*this));
}

template<typename SerFn, typename Acceptor>
inline void xdr_istream::accept_collection(SerFn fn, Acceptor acceptor) {
  accept_collection_n(get_uint32(),
      std::forward<SerFn>(fn),
      std::forward<Acceptor>(acceptor));
}


inline void xdr_ostream::put_void() {
  return;
}

inline void xdr_ostream::put_bool(bool v) {
  put_uint32(v ? 1u : 0u);
}

inline void xdr_ostream::put_int8(std::int8_t v) {
  put_int32(v);
}

inline void xdr_ostream::put_uint8(std::uint8_t v) {
  put_uint32(v);
}

inline void xdr_ostream::put_int16(std::int16_t v) {
  put_int32(v);
}

inline void xdr_ostream::put_uint16(std::uint16_t v) {
  put_uint32(v);
}

inline void xdr_ostream::put_int32(std::int32_t v) {
  if (v >= 0)
    put_uint32(static_cast<std::int32_t>(v));
  else
    put_uint32(static_cast<std::int32_t>(v + 0x80000000) + 0x80000000u);
}

inline void xdr_ostream::put_uint32(std::uint32_t v) {
  const std::uint32_t i = boost::endian::native_to_big(v);
  put_raw_bytes(&i, sizeof(i));
}

inline void xdr_ostream::put_int64(std::int64_t v) {
  if (v >= 0) {
    put_uint64(static_cast<std::uint64_t>(v));
  } else {
    put_uint64(static_cast<std::uint64_t>(v + 0x8000000000000000l) +
        0x8000000000000000ul);
  }
}

inline void xdr_ostream::put_flt32(float f) {
  static_assert(std::numeric_limits<float>::is_iec559,
      "require IEEE 754 layout.");
  static_assert(sizeof(float) == sizeof(std::uint32_t),
      "expecting uint32 and float to have same size");

  std::uint32_t tmp;
  std::memcpy(&tmp, &f, sizeof(tmp));
  put_uint32(tmp);
}

inline void xdr_ostream::put_flt64(double f) {
  static_assert(std::numeric_limits<double>::is_iec559,
      "require IEEE 754 layout.");
  static_assert(sizeof(double) == sizeof(std::uint64_t),
      "expecting uint32 and float to have same size");

  std::uint64_t tmp;
  std::memcpy(&tmp, &f, sizeof(tmp));
  put_uint64(tmp);
}

inline void xdr_ostream::put_uint64(std::uint64_t v) {
  const std::uint64_t i = boost::endian::native_to_big(v);
  put_raw_bytes(&i, sizeof(i));
}

#if __has_include(<string_view>)
inline void xdr_ostream::put_string(std::string_view s) {
  if (s.length() > 0xffffffffu)
    throw xdr_exception();

  put_uint32(static_cast<uint32_t>(s.length()));
  put_raw_bytes(s.data(), s.length());
  if (s.length() % 4u != 0u) put_padding(4u - s.length() % 4u);
}
#else
inline void xdr_ostream::put_string(const char* s) {
  std::size_t len = std::strlen(s);
  if (len > 0xffffffffu)
    throw xdr_exception();

  put_uint32(static_cast<uint32_t>(len));
  put_raw_bytes(s, len);
  if (len % 4u != 0u) put_padding(4u - len % 4u);
}
#endif

template<typename Alloc>
inline void xdr_ostream::put_string(
    const std::basic_string<char, std::char_traits<char>, Alloc>& s) {
  if (s.length() > 0xffffffffu)
    throw xdr_exception();

  put_uint32(static_cast<uint32_t>(s.length()));
  put_raw_bytes(s.data(), s.length());
  if (s.length() % 4u != 0u) put_padding(4u - s.length() % 4);
}

template<typename Alloc>
inline void xdr_ostream::put_opaque_n(std::size_t len,
    const std::vector<std::uint8_t, Alloc>& v) {
  if (v.size() != len)
    throw xdr_exception();

  put_raw_bytes(v.data(), len);
  if (v.size() % 4u != 0u) put_padding(4u - v.size() % 4);
}

template<typename Alloc>
inline void xdr_ostream::put_opaque(
    const std::vector<std::uint8_t, Alloc>& v) {
  if (v.size() > 0xffffffffu)
    throw xdr_exception();
  const std::uint32_t len = v.size();

  put_int32(len);
  put_opaque_n(len, v);
}

template<std::size_t Len>
inline void xdr_ostream::put_array(const std::array<std::uint8_t, Len>& arr) {
  put_raw_bytes(arr.data(), arr.size());
  if (arr.size() % 4u != 0u) put_padding(4u - arr.size() % 4);
}

template<typename SerFn, typename Iter>
inline auto xdr_ostream::put_collection_n(std::size_t len, SerFn fn, Iter iter)
-> Iter {
  for (std::size_t i = 0; i < len; ++i, ++iter)
    fn(*this, *iter);
  return iter;
}

template<typename SerFn, typename Iter>
inline auto xdr_ostream::put_collection(SerFn fn, Iter begin, Iter end)
-> std::enable_if_t<
    std::is_base_of<
      std::forward_iterator_tag,
      typename std::iterator_traits<Iter>::iterator_category>::value,
    void> {
  const auto len = std::distance(begin, end);
  if (len < 0 || len > 0xffffffffu)
    throw xdr_exception();
  put_uint32(static_cast<std::uint32_t>(len));
  std::for_each(begin, end, [this, &fn](const auto& arg) { fn(*this, arg); });
}

template<typename SerFn, typename Iter>
inline auto xdr_ostream::put_collection(SerFn fn, Iter begin, Iter end)
-> std::enable_if_t<
    !std::is_base_of<
      std::forward_iterator_tag,
      typename std::iterator_traits<Iter>::iterator_category>::value,
    void> {
  std::uint32_t len = 0;
  xdr_bytevector_ostream<> buffer;
  std::for_each(begin, end,
      [&](const auto& v) {
        if (len == 0xffffffffu)
          throw xdr_exception();
        fn(buffer, v);
        ++len;
      });

  put_uint32(static_cast<std::uint32_t>(len));
  put_raw_bytes(buffer.data(), buffer.size());
}

inline void xdr_ostream::put_padding(std::size_t n) {
  assert(n < 4);
  std::array<std::uint8_t, 4> pad;
  std::fill(pad.begin(), pad.end(), static_cast<std::uint8_t>(0u));
  put_raw_bytes(pad.data(), n);
}


template<typename Alloc>
inline xdr_bytevector_ostream<Alloc>::xdr_bytevector_ostream(
    xdr_bytevector_ostream&& o) noexcept
: xdr_ostream(std::move(o)),
  v_(std::move(o.v_))
{}

template<typename Alloc>
inline xdr_bytevector_ostream<Alloc>::xdr_bytevector_ostream(
    const Alloc& alloc)
: v_(alloc)
{}

template<typename Alloc>
inline auto xdr_bytevector_ostream<Alloc>::data() noexcept
-> std::uint8_t* {
  return v_.data();
}

template<typename Alloc>
inline auto xdr_bytevector_ostream<Alloc>::data() const noexcept
-> const std::uint8_t* {
  return v_.data();
}

template<typename Alloc>
inline auto xdr_bytevector_ostream<Alloc>::size() const noexcept -> size_type {
  return v_.size();
}

template<typename Alloc>
inline auto xdr_bytevector_ostream<Alloc>::as_vector() noexcept
-> vector_type& {
  return v_;
}

template<typename Alloc>
inline auto xdr_bytevector_ostream<Alloc>::as_vector() const noexcept
-> const vector_type& {
  return v_;
}

template<typename Alloc>
inline auto xdr_bytevector_ostream<Alloc>::put_raw_bytes(const void* buf,
    std::size_t len)
-> void {
  std::copy_n(reinterpret_cast<const std::uint8_t*>(buf), len,
      std::back_inserter(v_));
}


}} /* namespace monsoon::xdr */

#endif /* MONSOON_XDR_XDR_INL_H */
