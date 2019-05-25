#include <monsoon/history/dir/io/txfile.h>
#include <algorithm>
#include <cstring>
#include <iterator>

namespace monsoon::history::io {


auto txfile::replacement_map::read_at(monsoon::io::fd::offset_type off, void* buf, std::size_t& nbytes) const -> std::size_t {
  // Find the first region *after* off.
  auto iter = map_.upper_bound(off);
  assert(iter == map_.end() || iter->first > off);

  // Update nbytes to be clipped at next-region-start.
  if (iter != map_.end() && nbytes > iter->first - off)
    nbytes = iter->first - off;

  // If the first region *after* off is the very first in the list, we can't
  // read anything here.
  if (iter == map_.begin()) return 0;
  // Change iter to point at the at-or-before entry.
  --iter;

  // Validate assertions.
  assert(iter->first <= off);
  // If iter points at a before entry, this read can't be satisfied here.
  if (iter->first + iter->second.size() <= off) return 0;

  // Compute local offset into the *at* entry.
  const std::size_t local_off = off - iter->first;
  // And bytes that we can thus read.
  const std::size_t rlen = std::min(nbytes, std::size_t(iter->second.size() - local_off));
  // Validate our assertions: we must read something unless requested not to read anything.
  assert(rlen > 0 || nbytes == 0);
  // Do the read.
  std::memcpy(buf, iter->second.data() + local_off, rlen);
  return rlen; // Indicate we actually read something.
}

auto txfile::replacement_map::write_at(monsoon::io::fd::offset_type off, const void* buf, std::size_t nbytes) -> std::size_t {
  const monsoon::io::fd::offset_type end_off = off + nbytes;
  if (end_off < off) throw std::overflow_error("replacement_map: off + nbytes");

  // Find the first region *after* off.
  map_type::iterator succ = map_.upper_bound(off);
  assert(succ == map_.end() || succ->first > off);

  // Find the region at-or-before off.
  const map_type::iterator pred = (succ == map_.begin() ? map_.end() : std::prev(succ));
  assert(pred == map_.end() || pred->first <= off);

  // Discard successor if it does not intersect with the written region.
  if (succ != map_.end() && off + nbytes < succ->first)
    succ = map_.end();

  // Overlap with pred, replace the data in pred as appropriate using a short write.
  if (pred != map_.end() && pred->first + pred->second.size() > off) {
    const std::size_t local_off = off - pred->first;
    const auto wlen = std::min(pred->second.size() - local_off, nbytes);
    assert(wlen > 0);
    assert(local_off + wlen <= pred->second.size());
    std::copy_n(reinterpret_cast<const uint8_t*>(buf), wlen, pred->second.begin() + local_off);
    return wlen;
  }

  // Create a vector to hold information covering the gap until succ.
  map_type::mapped_type vector;
  if (succ != map_.end() && end_off > succ->first)
    nbytes = succ->first - off;
  assert(vector.max_size() > 0);
  if (nbytes > vector.max_size()) nbytes = vector.max_size();
  for (;;) {
    // Try to reduce allocation sizes if the allocations fail.
    // This should be very rare.
    try {
      vector.reserve(nbytes);
      break; // GUARD: break once reservation succeeds.
    } catch (const std::bad_alloc&) {
      if (nbytes <= 1u) throw;
      nbytes /= 2u;
    }
  }
  const std::size_t wlen = nbytes;
  std::copy_n(reinterpret_cast<const uint8_t*>(buf), wlen, std::back_inserter(vector));

  // Link the vector into the map at the given offset.
  assert(map_.find(off) == map_.end());
  map_type::iterator inserted;
  map_.insert(succ, map_type::value_type(off, std::move(vector)));
  return wlen;
}


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
