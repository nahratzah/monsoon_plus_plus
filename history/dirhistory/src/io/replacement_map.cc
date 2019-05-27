#include <monsoon/history/dir/io/replacement_map.h>
#include <memory>

namespace monsoon::history::io {


replacement_map::~replacement_map() noexcept {
  map_.clear_and_dispose([](entry_type* ptr) { delete ptr; });
}

auto replacement_map::read_at(monsoon::io::fd::offset_type off, void* buf, std::size_t& nbytes) const -> std::size_t {
  // Find the first region *after* off.
  auto iter = map_.upper_bound(off);
  assert(iter == map_.end() || iter->begin_offset() > off);

  // If the first region *after* off is the very first in the list, we can't
  // read anything here.
  // Same if the predecessor does not intersect the requested offset.
  if (iter == map_.begin() || std::prev(iter)->end_offset() <= off) {
    // Update nbytes to be clipped at next-region-start.
    if (nbytes > iter->begin_offset() - off) nbytes = iter->begin_offset() - off;
    return 0;
  }
  // Change iter to point at the at-or-before entry.
  --iter;

  // Validate assertions.
  assert(iter->begin_offset() <= off);

  // Compute local offset into the *at* entry.
  const std::size_t local_off = off - iter->begin_offset();
  // And bytes that we can thus read.
  const std::size_t rlen = std::min(nbytes, std::size_t(iter->size() - local_off));
  // Validate our assertions: we must read something unless requested not to read anything.
  assert(rlen > 0 || nbytes == 0);
  // Do the read.
  std::memcpy(buf, reinterpret_cast<const std::uint8_t*>(iter->data()) + local_off, rlen);
  return rlen; // Indicate we actually read something.
}

auto replacement_map::write_at(monsoon::io::fd::offset_type off, const void* buf, std::size_t nbytes, bool overwrite) -> tx {
  if (overwrite)
    return write_at_with_overwrite_(off, buf, nbytes);
  else
    return write_at_without_overwrite_(off, buf, nbytes);
}

auto replacement_map::write_at_from_file(monsoon::io::fd::offset_type off, const monsoon::io::fd& fd, monsoon::io::fd::offset_type fd_off, std::size_t nbytes, bool overwrite) -> tx {
  auto buffer = std::make_unique<std::uint8_t[]>(nbytes);

  auto fd_nbytes = nbytes;
  std::uint8_t* buffer_pos = buffer.get();
  while (fd_nbytes > 0) {
    const auto rlen = fd.read_at(fd_off, buffer_pos, fd_nbytes);
    buffer_pos += rlen;
    fd_off += rlen;
    fd_nbytes -= rlen;
  }

  return write_at(off, buffer.get(), nbytes, overwrite);
}

auto replacement_map::write_at_with_overwrite_(monsoon::io::fd::offset_type off, const void* buf, std::size_t nbytes) -> tx {
  const monsoon::io::fd::offset_type end_off = off + nbytes;
  if (end_off < off) throw std::overflow_error("replacement_map: off + nbytes");

  tx t;
  t.map_ = &map_;
  std::size_t bytes_from_pred = 0, bytes_from_succ = 0;

  // Find the first region *after* off.
  map_type::iterator succ = map_.upper_bound(off);
  assert(succ == map_.end() || succ->begin_offset() > off);

  // Find the region at-or-before off.
  const map_type::iterator pred = (succ == map_.begin() ? map_.end() : std::prev(succ));
  assert(pred == map_.end() || pred->begin_offset() <= off);

  // We may need to include some bytes from pred.
  if (pred != map_.end() && pred->end_offset() > off) {
    t.to_erase_.push_back(pred);
    bytes_from_pred = off - pred->begin_offset();
  }

  if (pred != map_.end() && pred->end_offset() > end_off) {
    // Handle the case where pred extends both sides.
    succ = pred;
    bytes_from_succ = succ->end_offset() - end_off;
  } else {
    // Mark any full-replaced entries as to-be-erased.
    // Move succ forward until it's the first not-fully-replaced entry after off.
    while (succ != map_.end() && succ->end_offset() <= end_off)
      t.to_erase_.push_back(succ++);

    // We may need to include bytes from succ.
    if (succ != map_.end() && succ->begin_offset() < end_off) {
      t.to_erase_.push_back(succ);
      bytes_from_succ = succ->end_offset() - end_off;
    }
  }

  // Reserve at least some bytes into the vector.
  vector_type vector;
  const std::uint8_t* pred_buf = reinterpret_cast<const std::uint8_t*>(pred->data());
  const std::uint8_t* byte_buf = reinterpret_cast<const std::uint8_t*>(buf);
  const std::uint8_t* succ_buf = reinterpret_cast<const std::uint8_t*>(succ->data()) + succ->size() - bytes_from_succ;
  off -= bytes_from_pred;
  while (bytes_from_succ > 0 || nbytes > 0 || bytes_from_pred > 0) {
    const std::size_t Max = vector.max_size();
    std::size_t to_reserve = bytes_from_pred;
    if (Max - to_reserve < nbytes) // overflow case
      to_reserve = Max;
    else
      to_reserve += nbytes;
    if (Max - to_reserve < bytes_from_succ) // overflow case
      to_reserve = Max;
    else
      to_reserve += bytes_from_succ;

    for (;;) {
      try {
        vector.resize(to_reserve);
        break;
      } catch (...) {
        if (to_reserve <= 1) throw;
        to_reserve /= 2;
      }
    }

    std::size_t wlen, written = 0;
    vector_type::iterator vector_pos = vector.begin();

    wlen = std::min(bytes_from_pred, to_reserve);
    vector_pos = std::copy_n(pred_buf, wlen, vector_pos);
    bytes_from_pred -= wlen;
    pred_buf += wlen;
    to_reserve -= wlen;
    written += wlen;

    wlen = std::min(nbytes, to_reserve);
    vector_pos = std::copy_n(byte_buf, wlen, vector_pos);
    nbytes -= wlen;
    byte_buf += wlen;
    to_reserve -= wlen;
    written += wlen;

    wlen = std::min(bytes_from_succ, to_reserve);
    vector_pos = std::copy_n(succ_buf, wlen, vector_pos);
    bytes_from_succ -= wlen;
    succ_buf += wlen;
    to_reserve -= wlen;
    written += wlen;

    assert(vector_pos == vector.end());
    t.to_insert_.emplace_back(std::make_unique<entry_type>(off, std::move(vector)));
    off += written;
  }

  return t;
}

auto replacement_map::write_at_without_overwrite_(monsoon::io::fd::offset_type off, const void* buf, std::size_t nbytes) -> tx {
  const monsoon::io::fd::offset_type end_off = off + nbytes;
  if (end_off < off) throw std::overflow_error("replacement_map: off + nbytes");

  tx t;
  t.map_ = &map_;

  /* We only write in the gaps, so we position 'iter' and 'iter_succ' such that
   * they are the delimiters of the first gap at/after 'off'.
   */
  while (off < end_off) {
    // Find the first region *after* off.
    const map_type::iterator succ = map_.upper_bound(off);
    assert(succ == map_.end() || succ->begin_offset() > off);

    // Find the region at-or-before off.
    map_type::iterator iter = (succ == map_.begin() ? map_.end() : std::prev(succ));
    assert(iter == map_.end() || iter->begin_offset() <= off);
    if (iter != map_.end() && iter->end_offset() >= end_off) break; // GUARD: stop if there are no intersecting gaps.
    assert(iter->end_offset() < end_off);

    if (iter != map_.end() && iter->end_offset() > off) {
      const monsoon::io::fd::offset_type skip = iter->end_offset() - off;
      off += skip;
      buf = reinterpret_cast<const std::uint8_t*>(buf) + skip;
    }
    assert(iter == map_.end() || iter->end_offset() <= off);

    map_type::iterator iter_succ = (iter == map_.end() ? map_.end() : std::next(iter));
    if (iter_succ->begin_offset() >= end_off) iter_succ = map_.end();
    assert(iter_succ == map_.end() || iter_succ->begin_offset() > off);

    monsoon::io::fd::offset_type write_end_off = (iter_succ == map_.end() ? end_off : iter_succ->begin_offset());
    assert(write_end_off <= end_off);

    while (off < write_end_off) {
      vector_type vector;
      vector_type::size_type to_reserve = vector.max_size();
      if (to_reserve > write_end_off - off) to_reserve = write_end_off - off;
      for (;;) {
        try {
          vector.reserve(to_reserve);
          break;
        } catch (const std::bad_alloc&) {
          if (to_reserve <= 1u) throw;
          to_reserve /= 2u;
        }
      }

      std::copy_n(reinterpret_cast<const std::uint8_t*>(buf), to_reserve, std::back_inserter(vector));
      t.to_insert_.emplace_back(std::make_unique<entry_type>(off, std::move(vector)));
      off += to_reserve;
      buf = reinterpret_cast<const std::uint8_t*>(buf) + to_reserve;
    }

    assert(off == write_end_off);
    iter = std::move(iter_succ);
  }

  return t;
}


replacement_map::tx::~tx() noexcept = default;

void replacement_map::tx::commit() noexcept {
  assert(map_ != nullptr);

  std::for_each(
      std::move_iterator(to_erase_.begin()),
      std::move_iterator(to_erase_.end()),
      [this](map_type::iterator&& iter) {
        map_->erase_and_dispose(iter, [](entry_type* ptr) { delete ptr; });
      });

  std::for_each(
      std::move_iterator(to_insert_.begin()),
      std::move_iterator(to_insert_.end()),
      [this](std::unique_ptr<entry_type>&& entry) {
        bool inserted = false;
        map_type::iterator ipos;
        std::tie(ipos, inserted) = map_->insert(*entry);
        assert(inserted);
        entry.release();

        // Assert the inserted element causes no overlap.
        assert(ipos == map_->begin() || std::prev(ipos)->end_offset() <= ipos->begin_offset());
        assert(std::next(ipos) == map_->end() || std::next(ipos)->begin_offset() >= ipos->end_offset());
      });

  map_ = nullptr;
  to_erase_.clear();
  to_insert_.clear();
}


} /* namespace monsoon::history::io */
