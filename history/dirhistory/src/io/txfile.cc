#include <monsoon/history/dir/io/txfile.h>

namespace monsoon::history::io {


txfile::txfile(monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len)
: fd_(std::move(fd)),
  wal_(fd_, off, len)
{}

txfile::txfile([[maybe_unused]] create_tag tag, monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len)
: fd_(std::move(fd)),
  wal_(wal_region::create(fd_, off, len))
{}

txfile::~txfile() noexcept = default;

auto txfile::create(monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len) -> txfile {
  return txfile(create_tag(), std::move(fd), off, len);
}


} /* namespace monsoon::history::io */
