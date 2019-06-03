#ifndef MONSOON_LIMITED_STREAM_H
#define MONSOON_LIMITED_STREAM_H

#include <cstddef>
#include <type_traits>
#include <utility>
#include <monsoon/misc_export_.h>
#include <monsoon/io/fd.h>
#include <monsoon/io/stream.h>

namespace monsoon::io {


template<typename S>
class limited_stream_reader
: public S
{
  public:
  template<typename... Args>
  explicit limited_stream_reader(fd::size_type len, Args&&... args)
  noexcept(std::is_nothrow_constructible_v<S, Args...>)
  : S(std::forward<Args>(args)...),
    len_(len)
  {}

  std::size_t read(void* buf, std::size_t nbytes) override {
    if (nbytes > len_) nbytes = len_;

    const auto rlen = this->S::read(buf, nbytes);
    len_ -= rlen;
    return rlen;
  }

  bool at_end() const override {
    return len_ == 0u || this->S::at_end();
  }

  private:
  fd::size_type len_;
};


template<typename S>
class limited_stream_writer
: public S
{
  public:
  template<typename... Args>
  limited_stream_writer(fd::size_type len, Args&&... args)
  noexcept(std::is_nothrow_constructible_v<S, Args...>)
  : S(std::forward<Args>(args)...),
    len_(len)
  {}

  std::size_t write(const void* buf, std::size_t nbytes) override {
    if (nbytes > len_) nbytes = len_;

    const auto wlen = this->S::write(buf, nbytes);
    len_ -= wlen;
    return wlen;
  }

  private:
  S s_;
  fd::size_type len_;
};


} /* namespace monsoon::io */

#endif /* MONSOON_LIMITED_STREAM_H */
