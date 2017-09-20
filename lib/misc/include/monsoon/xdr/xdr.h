#ifndef MONSOON_XDR_XDR_H
#define MONSOON_XDR_XDR_H

#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#if __has_include(<string_view>)
# include <string_view>
#endif
#include <type_traits>
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
      auto get_opaque_n(std::size_t, const Alloc& = Alloc())
      -> std::vector<std::uint8_t, Alloc>;
  template<typename Alloc = std::allocator<std::uint8_t>>
      auto get_opaque(const Alloc& = Alloc())
      -> std::vector<std::uint8_t, Alloc>;

  template<typename C, typename SerFn>
      auto get_collection_n(std::size_t, SerFn, C&& = C()) -> C&&;
  template<typename C, typename SerFn>
      auto get_collection_n(std::size_t, SerFn, C&) -> C&;
  template<typename C, typename SerFn>
      auto get_collection(SerFn, C&& = C()) -> C&&;
  template<typename C, typename SerFn>
      auto get_collection(SerFn, C&) -> C&;

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
      void put_opaque_n(std::size_t, const std::vector<std::uint8_t, Alloc>&);
  template<typename Alloc = std::allocator<uint8_t>>
      void put_opaque(const std::vector<std::uint8_t, Alloc>&);

  template<typename SerFn, typename Iter>
      auto put_collection_n(std::size_t, SerFn, Iter) -> Iter;
  template<typename SerFn, typename Iter>
      auto put_collection(SerFn, Iter, Iter)
      -> std::enable_if_t<
          std::is_base_of<
            std::forward_iterator_tag,
            typename std::iterator_traits<Iter>::iterator_category>::value,
          void>;
  template<typename SerFn, typename Iter>
      auto put_collection(SerFn, Iter, Iter)
      -> std::enable_if_t<
          !std::is_base_of<
            std::forward_iterator_tag,
            typename std::iterator_traits<Iter>::iterator_category>::value,
          void>;

 private:
  virtual void put_raw_bytes(const void*, std::size_t) = 0;
};

class xdr_exception
: public std::exception
{
 public:
  ~xdr_exception() override;
};


template<typename Alloc = std::allocator<std::uint8_t>>
class xdr_bytevector_ostream
: public xdr_ostream
{
 public:
  using vector_type = std::vector<std::uint8_t, Alloc>;
  using size_type = typename vector_type::size_type;

  xdr_bytevector_ostream() noexcept = default;
  xdr_bytevector_ostream(const xdr_bytevector_ostream&) = delete;
  xdr_bytevector_ostream(xdr_bytevector_ostream&&) noexcept;
  xdr_bytevector_ostream(const Alloc&);
  ~xdr_bytevector_ostream() noexcept override = default;

  std::uint8_t* data() noexcept;
  const std::uint8_t* data() const noexcept;
  size_type size() const noexcept;

  vector_type& get_vector() noexcept;
  const vector_type& get_vector() const noexcept;

 private:
  void put_raw_bytes(const void*, std::size_t) override;

  vector_type v_;
};


}} /* namespace monsoon::xdr */

#include "xdr-inl.h"

#endif /* MONSOON_XDR_XDR_H */
