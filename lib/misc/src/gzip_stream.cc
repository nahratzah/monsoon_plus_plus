#include <monsoon/gzip_stream.h>
#include <array>
#include <cassert>
#include <cerrno>
#include <new>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <zlib.h>

namespace monsoon {


bool is_gzip_file(stream_reader& file) {
  std::array<std::uint8_t, 2> hdr;
  std::size_t off = 0;

  while (off < hdr.size()) {
    const std::size_t rlen = file.read(hdr.data() + off, hdr.size() - off);
    if (rlen == 0) return false; // Too short to be a zip file.
    off += rlen;
  }

  return hdr == std::array<std::uint8_t, 2>{{ 0x1f, 0x8b }};
}


enum z_dir_ {
  DEFLATE,
  INFLATE
};

constexpr int WINDOW_BITS = 15;
constexpr int GZIP_HEADER_TRAILER = 16;
constexpr int DFL_MEMLEVEL = 8;
constexpr int out_buffer_size = 128 * 1024;
constexpr int in_buffer_size = 128 * 1024;

static void handle_zlib_error_(int ret) {
  if (ret > 0) return; // normal event

  switch (ret) {
    default:
      throw std::logic_error("unrecognized zlib error value");
    case Z_OK:
      return;
    case Z_ERRNO:
      throw std::system_error(errno, std::system_category());
    case Z_STREAM_ERROR:
      throw std::runtime_error("stream corrupted");
    case Z_DATA_ERROR:
      throw std::runtime_error("missing zlib data");
    case Z_MEM_ERROR:
      throw std::bad_alloc();
    case Z_BUF_ERROR:
      throw std::runtime_error("buffer error");
    case Z_VERSION_ERROR:
      throw std::logic_error("zlib version mismatch");
  }
}


/** A tiny struct, so we can avoid including zlib.h in the header file. */
struct z_stream_
: public z_stream
{
  inline z_stream_()
  : dir_(INFLATE)
  {
    this->zalloc = Z_NULL;
    this->zfree = Z_NULL;
    this->opaque = Z_NULL;
    this->avail_in = 0;
    this->next_in = nullptr;
    this->avail_out = 0;
    this->next_out = nullptr;

    auto ret = inflateInit2(
        this,
        WINDOW_BITS | GZIP_HEADER_TRAILER);
    handle_zlib_error_(ret);
  }

  inline z_stream_(int level)
  : dir_(DEFLATE)
  {
    if (!(level == Z_NO_COMPRESSION ||
            level == Z_DEFAULT_COMPRESSION ||
            (level >= Z_BEST_SPEED && level <= Z_BEST_COMPRESSION)))
      throw std::invalid_argument("level");

    this->zalloc = Z_NULL;
    this->zfree = Z_NULL;
    this->opaque = Z_NULL;
    this->avail_in = 0;
    this->next_in = nullptr;
    this->avail_out = 0;
    this->next_out = nullptr;

    auto ret = deflateInit2(
        this,
        level,
        Z_DEFLATED,
        WINDOW_BITS | GZIP_HEADER_TRAILER,
        DFL_MEMLEVEL,
        Z_DEFAULT_STRATEGY);
    handle_zlib_error_(ret);
  }

  inline ~z_stream_() noexcept {
    switch (dir_) {
      case DEFLATE:
        deflateEnd(this);
      case INFLATE:
        inflateEnd(this);
    }
  }

  const z_dir_ dir_;
};


gzip_decompress_reader::gzip_decompress_reader()
: strm_(std::make_unique<z_stream_>())
{}

gzip_decompress_reader::~gzip_decompress_reader() noexcept {}

std::size_t gzip_decompress_reader::read(void* data, std::size_t len) {
  delayed_init_();

  strm_->avail_out = len;
  strm_->next_out = reinterpret_cast<Bytef*>(data);
  if (strm_->avail_in == 0) from_source_();

  int ret = inflate(&*strm_, Z_NO_FLUSH);
  if (ret == Z_NEED_DICT) ret = Z_DATA_ERROR; // We have no dict.
  assert(ret != Z_STREAM_ERROR);
  handle_zlib_error_(ret);

  return len - strm_->avail_out;
}

void gzip_decompress_reader::from_source_() {
  const auto rpos = std::copy(
      in_.end() - strm_->avail_in,
      in_.end(),
      in_.begin());
  in_.resize(in_.capacity());
  const auto rlen = reader_().read(&*rpos, in_.end() - rpos);
  in_.erase(rpos + rlen, in_.end());

  strm_->avail_in = in_.size();
  strm_->next_in = in_.data();
}

void gzip_decompress_reader::delayed_init_() {
  if (in_.capacity() == 0) in_.reserve(in_buffer_size);
}


gzip_compress_writer::gzip_compress_writer()
: gzip_compress_writer(Z_DEFAULT_COMPRESSION)
{}

gzip_compress_writer::gzip_compress_writer(int level)
: strm_(std::make_unique<z_stream_>(level))
{}

gzip_compress_writer::gzip_compress_writer(gzip_compress_writer&& o) noexcept
: strm_(std::move(o.strm_)),
  out_(std::move(o.out_))
{}

gzip_compress_writer::~gzip_compress_writer() noexcept {}

std::size_t gzip_compress_writer::write(const void* data, std::size_t len) {
  delayed_init_();

  strm_->avail_in = len;
  strm_->next_in = reinterpret_cast<Bytef*>(const_cast<void*>(data));

  const int ret = deflate(&*strm_, Z_NO_FLUSH);
  assert(ret != Z_STREAM_ERROR);
  handle_zlib_error_(ret);

  if (strm_->avail_out == 0) to_sink_();

  return len - strm_->avail_in;
}

void gzip_compress_writer::close() {
  delayed_init_();

  strm_->avail_in = 0;
  strm_->next_in = nullptr;

  int ret;
  do {
    ret = deflate(&*strm_, Z_FINISH);
    assert(ret != Z_STREAM_ERROR);
    to_sink_();
  } while (ret != Z_STREAM_END);

  handle_zlib_error_(ret);
  strm_.reset();
}

void gzip_compress_writer::to_sink_() {
  const std::size_t to_be_written = out_.size() - strm_->avail_out;
  const std::size_t wlen = writer_().write(out_.data(), to_be_written);
  const auto next_out = std::copy(
      out_.begin() + wlen,
      out_.begin() + to_be_written,
      out_.begin());
  strm_->avail_out = out_.end() - next_out;
  strm_->next_out = &*next_out;
}

void gzip_compress_writer::delayed_init_() {
  if (out_.empty()) {
    out_.resize(out_buffer_size);
    strm_->avail_out = out_.size();
    strm_->next_out = out_.data();
  }
}


} /* namespace monsoon */
