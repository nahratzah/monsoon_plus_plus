#include <monsoon/gzip_stream.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cerrno>
#include <new>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <zlib.h>

namespace monsoon {


bool is_gzip_file(stream_reader&& file) {
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
constexpr std::size_t out_buffer_size = 128 * 1024;
constexpr std::size_t in_buffer_size = 128 * 1024;
constexpr std::size_t in_pending_size = 128 * 1024;

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


basic_gzip_decompress_reader::basic_gzip_decompress_reader(bool verify_stream)
: strm_(std::make_unique<z_stream_>()),
  verify_stream_(verify_stream)
{}

basic_gzip_decompress_reader::~basic_gzip_decompress_reader() noexcept {}

basic_gzip_decompress_reader::basic_gzip_decompress_reader(
    basic_gzip_decompress_reader&& o) noexcept
: strm_(std::move(o.strm_)),
  in_(std::move(o.in_)),
  pending_(std::move(o.pending_)),
  pending_off_(std::move(o.pending_off_)),
  stream_end_seen_(std::move(o.stream_end_seen_)),
  verify_stream_(std::move(o.verify_stream_))
{}

basic_gzip_decompress_reader& basic_gzip_decompress_reader::operator=(
    basic_gzip_decompress_reader&& o) noexcept {
  strm_ = std::move(o.strm_);
  in_ = std::move(o.in_);
  return *this;
}

std::size_t basic_gzip_decompress_reader::read(void* data, std::size_t len) {
  if (!strm_) throw std::logic_error("stream closed");

  if (len == 0) return 0;
  if (pending_.empty()) {
    if (stream_end_seen_) return 0;
    if (len >= in_pending_size) return read_(data, len);

    fill_pending_();
    assert(!pending_.empty() || stream_end_seen_);
    if (pending_.empty()) return 0;
  }

  assert(pending_off_ < pending_.size());
  const std::size_t plen = std::min(pending_.size() - pending_off_, len);
  std::copy_n(pending_.begin() + pending_off_, plen,
      reinterpret_cast<std::uint8_t*>(data));
  pending_off_ += plen;
  len -= plen;

  if (pending_off_ == pending_.size()) {
    pending_.clear();
    pending_off_ = 0;
  }

  return plen;
}

bool basic_gzip_decompress_reader::at_end() const {
  if (!strm_) throw std::logic_error("stream closed");

  if (pending_.empty() && !stream_end_seen_) fill_pending_();
  return pending_.empty() && stream_end_seen_;
}

void basic_gzip_decompress_reader::close() {
  if (!strm_) throw std::logic_error("stream closed");

  if (verify_stream_) {
    while (!stream_end_seen_) {
      pending_.clear(); // Discard data.
      pending_off_ = 0;
      fill_pending_();
    }
  }

  strm_.reset();
  reader_().close();
}

void basic_gzip_decompress_reader::from_source_() const {
  const auto rpos = std::copy(
      in_.end() - strm_->avail_in,
      in_.end(),
      in_.begin());
  in_.resize(in_.capacity());
  const auto rlen = const_cast<basic_gzip_decompress_reader&>(*this)
      .reader_()
      .read(&*rpos, in_.end() - rpos);
  in_.erase(rpos + rlen, in_.end());

  strm_->avail_in = in_.size();
  strm_->next_in = in_.data();
}

void basic_gzip_decompress_reader::delayed_init_() const {
  if (in_.capacity() == 0) in_.reserve(in_buffer_size);
}

void basic_gzip_decompress_reader::fill_pending_() const {
  assert(pending_.empty());
  assert(pending_off_ == 0);
  pending_.resize(in_pending_size);
  pending_.resize(read_(pending_.data(), pending_.size()));
}

std::size_t basic_gzip_decompress_reader::read_(
    void* data, std::size_t len) const {
  delayed_init_();

  strm_->avail_out = len;
  strm_->next_out = reinterpret_cast<Bytef*>(data);
  if (strm_->avail_in == 0) from_source_();

  int ret = inflate(&*strm_, Z_NO_FLUSH);
  if (ret == Z_NEED_DICT) ret = Z_DATA_ERROR; // We have no dict.
  assert(ret != Z_STREAM_ERROR);
  handle_zlib_error_(ret);
  if (ret == Z_STREAM_END) stream_end_seen_ = true;

  return len - strm_->avail_out;
}


basic_gzip_compress_writer::basic_gzip_compress_writer()
: basic_gzip_compress_writer(Z_DEFAULT_COMPRESSION)
{}

basic_gzip_compress_writer::basic_gzip_compress_writer(int level)
: strm_(std::make_unique<z_stream_>(level))
{}

basic_gzip_compress_writer::basic_gzip_compress_writer(
    basic_gzip_compress_writer&& o) noexcept
: strm_(std::move(o.strm_)),
  out_(std::move(o.out_))
{}

basic_gzip_compress_writer& basic_gzip_compress_writer::operator=(
    basic_gzip_compress_writer&& o) noexcept {
  strm_ = std::move(o.strm_);
  out_ = std::move(o.out_);
  return *this;
}

basic_gzip_compress_writer::~basic_gzip_compress_writer() noexcept {}

std::size_t basic_gzip_compress_writer::write(const void* data, std::size_t len) {
  if (!strm_) throw std::logic_error("stream closed");
  delayed_init_();

  strm_->avail_in = len;
  strm_->next_in = reinterpret_cast<Bytef*>(const_cast<void*>(data));

  const int ret = deflate(&*strm_, Z_NO_FLUSH);
  assert(ret != Z_STREAM_ERROR);
  handle_zlib_error_(ret);

  if (strm_->avail_out == 0) to_sink_();

  return len - strm_->avail_in;
}

void basic_gzip_compress_writer::close() {
  if (!strm_) throw std::logic_error("stream closed");
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
  writer_().close();
}

void basic_gzip_compress_writer::to_sink_() {
  const std::size_t to_be_written = out_.size() - strm_->avail_out;
  const std::size_t wlen = writer_().write(out_.data(), to_be_written);
  const auto next_out = std::copy(
      out_.begin() + wlen,
      out_.begin() + to_be_written,
      out_.begin());
  strm_->avail_out = out_.end() - next_out;
  strm_->next_out = &*next_out;
}

void basic_gzip_compress_writer::delayed_init_() {
  if (out_.empty()) {
    out_.resize(out_buffer_size);
    strm_->avail_out = out_.size();
    strm_->next_out = out_.data();
  }
}


} /* namespace monsoon */
