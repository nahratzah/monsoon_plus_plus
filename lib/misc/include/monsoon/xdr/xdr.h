#ifndef MONSOON_XDR_XDR_H
#define MONSOON_XDR_XDR_H

#include <cstdint>
#include <string>
#if __has_include(<string_view>)
# include <string_view>
#endif
#include <vector>

namespace monsoon {
namespace xdr {


class xdr_istream {
 public:
  virtual ~xdr_istream() noexcept;

  void get_void();
  bool get_bool();
  std::int8_t get_int8();
  std::uint8_t get_uint8();
  std::int16_t get_int16();
  std::uint16_t get_uint16();
  std::int32_t get_int32();
  std::uint32_t get_uint32();
  std::int64_t get_int64();
  std::uint64_t get_uint64();

  template<typename Alloc = std::allocator<char>>
      auto get_string(const Alloc& = Alloc())
      -> std::basic_string<char, std::char_traits<char>, Alloc>;

  template<typename Alloc = std::allocator<std::uint8_t>>
      auto get_opaque(std::size_t, const Alloc& = Alloc())
      -> std::vector<std::uint8_t, Alloc>;
  template<typename Alloc = std::allocator<std::uint8_t>>
      auto get_opaque(const Alloc& = Alloc())
      -> std::vector<std::uint8_t, Alloc>;

 private:
  virtual void get_raw_bytes(void*, std::size_t) = 0;
};

class xdr_ostream {
 public:
  virtual ~xdr_ostream() noexcept;

  void put_void();
  void put_bool(bool);
  void put_int8(std::int8_t);
  void put_uint8(std::uint8_t);
  void put_int16(std::int16_t);
  void put_uint16(std::uint16_t);
  void put_int32(std::int32_t);
  void put_uint32(std::uint32_t);
  void put_int64(std::int64_t);
  void put_uint64(std::uint64_t);

#if __has_include(<string_view>)
  void put_string(std::string_view);
#endif
  template<typename Alloc>
  void put_string(
      const std::basic_string<char, std::char_traits<char>, Alloc>&);

  template<typename Alloc = std::allocator<uint8_t>>
      void put_opaque(std::size_t, const std::vector<std::uint8_t, Alloc>&);
  template<typename Alloc = std::allocator<uint8_t>>
      void put_opaque(const std::vector<std::uint8_t, Alloc>&);

 private:
  virtual void put_raw_bytes(const void*, std::size_t) = 0;
};

class xdr_exception
: public std::exception
{
 public:
  ~xdr_exception() override;
};


}} /* namespace monsoon::xdr */

#include "xdr-inl.h"

#endif /* MONSOON_XDR_XDR_H */
