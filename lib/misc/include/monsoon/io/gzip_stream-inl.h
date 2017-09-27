#ifndef MONSOON_GZIP_STREAM_INL_H
#define MONSOON_GZIP_STREAM_INL_H

namespace monsoon {
namespace io {


template<typename Reader>
gzip_decompress_reader<Reader>::gzip_decompress_reader(Reader&& r)
: r_(std::move(r))
{}

template<typename Reader>
gzip_decompress_reader<Reader>::gzip_decompress_reader(Reader&& r,
    bool validate)
: basic_gzip_decompress_reader(validate),
  r_(std::move(r))
{}

template<typename Reader>
gzip_decompress_reader<Reader>::gzip_decompress_reader(
    gzip_decompress_reader&& o)
    noexcept(std::is_nothrow_move_constructible<Reader>())
: basic_gzip_decompress_reader(std::move(o)),
  r_(std::move(o.r_))
{}

template<typename Reader>
auto gzip_decompress_reader<Reader>::operator=(
    gzip_decompress_reader&& o)
    noexcept(std::is_nothrow_move_assignable<Reader>())
-> gzip_decompress_reader& {
  static_cast<gzip_decompress_reader&>(*this) = std::move(o);
  r_ = std::move(o.r_);
  return *this;
}

template<typename Reader>
stream_reader& gzip_decompress_reader<Reader>::reader_() {
  return r_;
}


template<typename Writer>
gzip_compress_writer<Writer>::gzip_compress_writer(Writer&& w)
: w_(std::move(w))
{}

template<typename Writer>
gzip_compress_writer<Writer>::gzip_compress_writer(Writer&& w, int level)
: basic_gzip_compress_writer(level),
  w_(std::move(w))
{}

template<typename Writer>
gzip_compress_writer<Writer>::gzip_compress_writer(gzip_compress_writer&& o)
    noexcept(std::is_nothrow_move_constructible<Writer>())
: basic_gzip_compress_writer(std::move(o)),
  w_(std::move(o.w_))
{}

template<typename Writer>
auto gzip_compress_writer<Writer>::operator=(gzip_compress_writer&& o)
    noexcept(std::is_nothrow_move_assignable<Writer>())
-> gzip_compress_writer& {
  static_cast<gzip_compress_writer&>(*this) = std::move(o);
  w_ = std::move(o.w_);
  return *this;
}

template<typename Writer>
stream_writer& gzip_compress_writer<Writer>::writer_() {
  return w_;
}


template<typename W>
auto gzip_compression(W&& writer, int level)
-> std::enable_if_t<std::is_base_of_v<stream_writer, W> && !std::is_const_v<W>,
    gzip_compress_writer<W>> {
  return gzip_compress_writer<W>(std::forward<W>(writer, level));
}

template<typename W>
auto gzip_compression(W&& writer)
-> std::enable_if_t<std::is_base_of_v<stream_writer, W> && !std::is_const_v<W>,
    gzip_compress_writer<W>> {
  return gzip_compress_writer<W>(std::forward<W>(writer));
}

template<typename R>
auto gzip_decompression(R&& reader)
-> std::enable_if_t<std::is_base_of_v<stream_writer, R> && !std::is_const_v<R>,
    gzip_decompress_reader<R>> {
  return gzip_decompress_reader<R>(std::forward<R>(reader));
}


}} /* namespace monsoon::io */

#endif /* MONSOON_GZIP_STREAM_INL_H */
