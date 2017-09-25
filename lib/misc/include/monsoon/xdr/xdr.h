#ifndef MONSOON_XDR_XDR_H
#define MONSOON_XDR_XDR_H

#include <monsoon/misc_export_.h>
#include <array>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include <optional>
#include <functional>

namespace monsoon {
namespace xdr {


class monsoon_misc_export_ xdr_istream {
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
  float get_flt32();
  double get_flt64();

  template<typename Alloc = std::allocator<char>>
      auto get_string(const Alloc& = Alloc())
      -> std::basic_string<char, std::char_traits<char>, Alloc>;

  template<typename Alloc = std::allocator<std::uint8_t>>
      auto get_opaque_n(std::size_t, const Alloc& = Alloc())
      -> std::vector<std::uint8_t, Alloc>;
  template<typename Alloc = std::allocator<std::uint8_t>>
      auto get_opaque(const Alloc& = Alloc())
      -> std::vector<std::uint8_t, Alloc>;

  template<std::size_t Len>
     auto get_array(std::array<std::uint8_t, Len>&)
     -> std::enable_if_t<Len % 4u == 0u, std::array<std::uint8_t, Len>&>;
  template<std::size_t Len>
     auto get_array(std::array<std::uint8_t, Len>&)
     -> std::enable_if_t<Len % 4u != 0u, std::array<std::uint8_t, Len>&>;
  template<std::size_t Len>
     auto get_array(std::array<std::uint8_t, Len>&&)
     -> std::array<std::uint8_t, Len>&&;
  template<std::size_t Len>
     auto get_array() -> std::array<std::uint8_t, Len>;

  template<typename C, typename SerFn>
      auto get_collection_n(std::size_t, SerFn, C&& = C()) -> C&&;
  template<typename C, typename SerFn>
      auto get_collection_n(std::size_t, SerFn, C&) -> C&;
  template<typename C, typename SerFn>
      auto get_collection(SerFn, C&& = C()) -> C&&;
  template<typename C, typename SerFn>
      auto get_collection(SerFn, C&) -> C&;
  template<typename SerFn, typename Acceptor>
      void accept_collection_n(std::size_t, SerFn, Acceptor);
  template<typename SerFn, typename Acceptor>
      void accept_collection(SerFn, Acceptor);

  template<typename SerFn>
      auto get_optional(SerFn)
      -> std::optional<decltype(
          std::invoke(std::declval<SerFn>(), std::declval<xdr_istream&>()))>;

  virtual bool at_end() const = 0;
  virtual void close() = 0;

 private:
  virtual void get_raw_bytes(void*, std::size_t) = 0;
};

class monsoon_misc_export_ xdr_ostream {
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
  void put_flt32(float);
  void put_flt64(double);

  void put_string(std::string_view);

  template<typename Alloc = std::allocator<uint8_t>>
      void put_opaque_n(std::size_t, const std::vector<std::uint8_t, Alloc>&);
  template<typename Alloc = std::allocator<uint8_t>>
      void put_opaque(const std::vector<std::uint8_t, Alloc>&);

  template<std::size_t Len>
      void put_array(const std::array<std::uint8_t, Len>&);

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

  void put_raw_data(const void*, std::size_t);

  virtual void close() = 0;

 private:
  virtual void put_raw_bytes(const void*, std::size_t) = 0;
  void put_padding(std::size_t);
};

class monsoon_misc_export_ xdr_exception
: public std::exception
{
 public:
  xdr_exception();
  xdr_exception(const xdr_exception&) = default;
  xdr_exception& operator=(const xdr_exception&) = default;
  xdr_exception(const char* what);
  ~xdr_exception() override;

  const char* what() const noexcept override;

 private:
  const char* what_ = nullptr;
};

class monsoon_misc_export_ xdr_stream_end
: public xdr_exception
{
 public:
  using xdr_exception::xdr_exception;
  ~xdr_stream_end() override;
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

  void close() override {}

  std::uint8_t* data() noexcept;
  const std::uint8_t* data() const noexcept;
  size_type size() const noexcept;

  vector_type& as_vector() noexcept;
  const vector_type& as_vector() const noexcept;
  void copy_to(xdr_ostream&) const;

 private:
  void put_raw_bytes(const void*, std::size_t) override;

  vector_type v_;
};


}} /* namespace monsoon::xdr */

#include "xdr-inl.h"

#endif /* MONSOON_XDR_XDR_H */
