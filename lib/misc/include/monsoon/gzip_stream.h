#ifndef MONSOON_GZIP_STREAM_H
#define MONSOON_GZIP_STREAM_H

#include <monsoon/stream.h>
#include <vector>

namespace monsoon {


struct z_stream_;

bool is_gzip_file(stream_reader&);

class gzip_decompress_reader
: public stream_reader
{
 public:
  gzip_decompress_reader();
  gzip_decompress_reader(const gzip_decompress_reader&) = delete;
  gzip_decompress_reader(gzip_decompress_reader&&);
  gzip_decompress_reader& operator=(const gzip_decompress_reader&) = delete;
  gzip_decompress_reader& operator=(gzip_decompress_reader&&) noexcept;
  ~gzip_decompress_reader() noexcept override;

  std::size_t read(void*, std::size_t) override;

 private:
  void from_source_();
  void delayed_init_();
  virtual stream_reader& reader_() = 0;

  std::unique_ptr<z_stream_> strm_;
  std::vector<std::uint8_t> in_;
};


class gzip_compress_writer
: public stream_writer
{
 public:
  gzip_compress_writer();
  explicit gzip_compress_writer(int level);
  gzip_compress_writer(const gzip_compress_writer&) = delete;
  gzip_compress_writer(gzip_compress_writer&&) noexcept;
  gzip_compress_writer& operator=(const gzip_compress_writer&) = delete;
  gzip_compress_writer& operator=(gzip_compress_writer&&) noexcept;
  ~gzip_compress_writer() noexcept override;

  std::size_t write(const void*, std::size_t) override;
  void close() override;

 private:
  void to_sink_();
  void delayed_init_();
  virtual stream_writer& writer_() = 0;

  std::unique_ptr<z_stream_> strm_;
  std::vector<std::uint8_t> out_;
};


} /* namespace monsoon */

#endif /* MONSOON_GZIP_STREAM_H */
