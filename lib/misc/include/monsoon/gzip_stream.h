#ifndef MONSOON_GZIP_STREAM_H
#define MONSOON_GZIP_STREAM_H

#include <monsoon/stream.h>
#include <type_traits>
#include <vector>

namespace monsoon {


struct z_stream_;

bool is_gzip_file(stream_reader&);

class basic_gzip_decompress_reader
: public stream_reader
{
 protected:
  basic_gzip_decompress_reader();
  basic_gzip_decompress_reader(const basic_gzip_decompress_reader&) = delete;
  basic_gzip_decompress_reader(basic_gzip_decompress_reader&&) noexcept;
  basic_gzip_decompress_reader& operator=(
      const basic_gzip_decompress_reader&) = delete;
  basic_gzip_decompress_reader& operator=(
      basic_gzip_decompress_reader&&) noexcept;

 public:
  ~basic_gzip_decompress_reader() noexcept override;

  std::size_t read(void*, std::size_t) override;

 private:
  void from_source_();
  void delayed_init_();
  virtual stream_reader& reader_() = 0;

  std::unique_ptr<z_stream_> strm_;
  std::vector<std::uint8_t> in_;
};


class basic_gzip_compress_writer
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
  void to_sink_();
  void delayed_init_();
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

  gzip_decompress_reader(Reader&& r)
  : r_(std::move(r))
  {}

  gzip_decompress_reader(gzip_decompress_reader&& o)
      noexcept(std::is_nothrow_move_constructible<Reader>())
  : basic_gzip_decompress_reader(std::move(o)),
    r_(std::move(o.r_))
  {}

  gzip_decompress_reader& operator=(gzip_decompress_reader&& o)
      noexcept(std::is_nothrow_move_assignable<Reader>()) {
    static_cast<gzip_decompress_reader&>(*this) = std::move(o);
    r_ = std::move(o.r_);
    return *this;
  }

 private:
  stream_reader& reader_() override { return r_; }

  Reader r_;
};


template<typename Writer>
class gzip_compress_writer
: public basic_gzip_compress_writer
{
 public:
  gzip_compress_writer() = default;

  gzip_compress_writer(Writer&& w)
  : w_(std::move(w))
  {}

  gzip_compress_writer(gzip_compress_writer&& o)
      noexcept(std::is_nothrow_move_constructible<Writer>())
  : gzip_compress_writer(std::move(o)),
    w_(std::move(o.w_))
  {}

  gzip_compress_writer& operator=(gzip_compress_writer&& o)
      noexcept(std::is_nothrow_move_assignable<Writer>()) {
    static_cast<gzip_compress_writer&>(*this) = std::move(o);
    w_ = std::move(o.w_);
    return *this;
  }

 private:
  stream_writer& writer_() override { return w_; }

  Writer w_;
};


} /* namespace monsoon */

#endif /* MONSOON_GZIP_STREAM_H */
