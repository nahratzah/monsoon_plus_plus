#ifndef MONSOON_GZIP_STREAM_H
#define MONSOON_GZIP_STREAM_H

#include <monsoon/misc_export_.h>
#include <monsoon/io/stream.h>
#include <type_traits>
#include <vector>

namespace monsoon {
namespace io {


struct z_stream_;

monsoon_misc_export_ bool is_gzip_file(stream_reader&&);

class monsoon_misc_export_ basic_gzip_decompress_reader
: public stream_reader
{
 protected:
  basic_gzip_decompress_reader(bool verify_stream = false);
  basic_gzip_decompress_reader(const basic_gzip_decompress_reader&) = delete;
  basic_gzip_decompress_reader(basic_gzip_decompress_reader&&) noexcept;
  basic_gzip_decompress_reader& operator=(
      const basic_gzip_decompress_reader&) = delete;
  basic_gzip_decompress_reader& operator=(
      basic_gzip_decompress_reader&&) noexcept;

 public:
  ~basic_gzip_decompress_reader() noexcept override;

  std::size_t read(void*, std::size_t) override;
  bool at_end() const override;
  void close() override;

 private:
  monsoon_misc_local_ void from_source_() const;
  monsoon_misc_local_ void delayed_init_() const;
  monsoon_misc_local_ void fill_pending_() const;
  monsoon_misc_local_ std::size_t read_(void*, std::size_t) const;
  virtual stream_reader& reader_() = 0;

  std::unique_ptr<z_stream_> strm_;
  mutable std::vector<std::uint8_t> in_;
  mutable std::vector<std::uint8_t> pending_;
  std::vector<std::uint8_t>::size_type pending_off_ = 0u;
  mutable bool stream_end_seen_ = false;
  bool verify_stream_ = false;
};


class monsoon_misc_export_ basic_gzip_compress_writer
: public stream_writer
{
 protected:
  basic_gzip_compress_writer();
  explicit basic_gzip_compress_writer(int level);
  basic_gzip_compress_writer(const basic_gzip_compress_writer&) = delete;
  basic_gzip_compress_writer(basic_gzip_compress_writer&&) noexcept;
  basic_gzip_compress_writer& operator=(
      const basic_gzip_compress_writer&) = delete;
  basic_gzip_compress_writer& operator=(
      basic_gzip_compress_writer&&) noexcept;

 public:
  ~basic_gzip_compress_writer() noexcept override;

  std::size_t write(const void*, std::size_t) override;
  void close() override;

 private:
  monsoon_misc_local_ void to_sink_();
  monsoon_misc_local_ void delayed_init_();
  virtual stream_writer& writer_() = 0;

  std::unique_ptr<z_stream_> strm_;
  std::vector<std::uint8_t> out_;
};


template<typename Reader>
class gzip_decompress_reader
: public basic_gzip_decompress_reader
{
 public:
  gzip_decompress_reader() = default;
  gzip_decompress_reader(Reader&&);
  gzip_decompress_reader(Reader&&, bool);
  gzip_decompress_reader(gzip_decompress_reader&&)
      noexcept(std::is_nothrow_move_constructible<Reader>());
  gzip_decompress_reader& operator=(gzip_decompress_reader&&)
      noexcept(std::is_nothrow_move_assignable<Reader>());

 private:
  stream_reader& reader_() override;

  Reader r_;
};


template<typename Writer>
class gzip_compress_writer
: public basic_gzip_compress_writer
{
 public:
  gzip_compress_writer() = default;
  gzip_compress_writer(Writer&&);
  gzip_compress_writer(Writer&&, int);
  gzip_compress_writer(gzip_compress_writer&&)
      noexcept(std::is_nothrow_move_constructible<Writer>());
  gzip_compress_writer& operator=(gzip_compress_writer&&)
      noexcept(std::is_nothrow_move_assignable<Writer>());

 private:
  stream_writer& writer_() override;

  Writer w_;
};

template<typename W>
auto gzip_compression(W&&, int)
-> std::enable_if_t<std::is_base_of_v<stream_writer, W> && !std::is_const_v<W>,
    gzip_compress_writer<W>>;

template<typename W>
auto gzip_compression(W&&)
-> std::enable_if_t<std::is_base_of_v<stream_writer, W> && !std::is_const_v<W>,
    gzip_compress_writer<W>>;

template<typename R>
auto gzip_decompression(R&&)
-> std::enable_if_t<std::is_base_of_v<stream_writer, R> && !std::is_const_v<R>,
    gzip_decompress_reader<R>>;


}} /* namespace monsoon::io */

#include "gzip_stream-inl.h"

#endif /* MONSOON_GZIP_STREAM_H */
