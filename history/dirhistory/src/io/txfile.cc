#include <monsoon/history/dir/io/txfile.h>

namespace monsoon::history::io {


txfile::txfile(monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len)
: fd_(std::move(fd)),
  wal_(fd_, off, len)
{}

txfile::~txfile() noexcept = default;


} /* namespace monsoon::history::io */
