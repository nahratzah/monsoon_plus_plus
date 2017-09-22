#ifndef MONSOON_POSITIONAL_STREAM_INL_H
#define MONSOON_POSITIONAL_STREAM_INL_H

namespace monsoon {


inline positional_reader::positional_reader(const fd& fd, fd::offset_type off)
noexcept
: fd_(&fd),
  off_(off)
{}

inline positional_writer::positional_writer(fd& fd, fd::offset_type off)
noexcept
: fd_(&fd),
  off_(off)
{}


} /* namespace monsoon */

#endif /* MONSOON_POSITIONAL_STREAM_INL_H */
