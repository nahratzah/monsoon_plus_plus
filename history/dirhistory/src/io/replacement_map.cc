#include <monsoon/history/dir/io/replacement_map.h>
#include <memory>
#include <stdexcept>

namespace monsoon::history::io {
namespace {


auto replacement_map_clone(const replacement_map::value_type& r) -> replacement_map::value_type* {
  return new replacement_map::value_type(r);
}

void replacement_map_dispose(replacement_map::value_type* ptr) {
  delete ptr;
}


}


replacement_map::replacement_map(const replacement_map& y)
: replacement_map() // So the destructor will be invoked on exception.
{
  map_.clone_from(
      y.map_,
      &replacement_map_clone,
      &replacement_map_dispose);
}

auto replacement_map::operator=(const replacement_map& y) -> replacement_map& {
  if (this == &y) [[unlikely]]
    return *this;

  map_.clone_from(
      y.map_,
      &replacement_map_clone,
      &replacement_map_dispose);

  return *this;
}

auto replacement_map::operator=(replacement_map&& y) noexcept -> replacement_map& {
  map_.swap(y.map_);
  map_.clear_and_dispose(&replacement_map_dispose);
  return *this;
}

replacement_map::~replacement_map() noexcept {
  map_.clear_and_dispose(&replacement_map_dispose);
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

  // Prepare a replacement for pred.
  if (bytes_from_pred > 0u) {
    auto new_pred = std::make_unique<value_type>(*pred);
    new_pred->keep_front(bytes_from_pred);
    t.to_insert_.emplace_back(std::move(new_pred));
  }

  // Prepare a replacement for succ.
  if (bytes_from_succ > 0u) {
    auto new_succ = std::make_unique<value_type>(*succ);
    new_succ->keep_back(bytes_from_succ);
    t.to_insert_.emplace_back(std::move(new_succ));
  }

  // Reserve at least some bytes into the vector.
  const std::uint8_t* byte_buf = reinterpret_cast<const std::uint8_t*>(buf);
  while (nbytes > 0) {
    std::unique_ptr<std::uint8_t[]> vector;
    std::size_t to_reserve = nbytes;

    for (;;) {
      try {
        vector = std::make_unique<std::uint8_t[]>(to_reserve);
        break;
      } catch (...) {
        if (to_reserve <= 1) throw;
        to_reserve /= 2;
      }
    }

    std::copy_n(byte_buf, to_reserve, vector.get());
    nbytes -= to_reserve;
    byte_buf += to_reserve;

    t.to_insert_.emplace_back(std::make_unique<value_type>(off, std::move(vector), to_reserve));
    off += to_reserve;
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
      std::unique_ptr<std::uint8_t[]> vector;
      std::size_t to_reserve = write_end_off - off;
      for (;;) {
        try {
          vector = std::make_unique<std::uint8_t[]>(to_reserve);
          break;
        } catch (const std::bad_alloc&) {
          if (to_reserve <= 1u) throw;
          to_reserve /= 2u;
        }
      }

      std::copy_n(reinterpret_cast<const std::uint8_t*>(buf), to_reserve, vector.get());
      t.to_insert_.emplace_back(std::make_unique<value_type>(off, std::move(vector), to_reserve));
      off += to_reserve;
      buf = reinterpret_cast<const std::uint8_t*>(buf) + to_reserve;
    }

    assert(off == write_end_off);
    iter = std::move(iter_succ);
  }

  return t;
}


auto replacement_map::value_type::pop_front(size_type n) -> value_type& {
  if (n > size_) throw std::overflow_error("replacement_map::value_type::pop_front");

  first += n;
#if __cpp_lib_shared_ptr_arrays >= 201611
  data_ = std::shared_ptr<const std::uint8_t[]>(data_, data_.get() + n);
#else
  data_ = std::shared_ptr<const std::uint8_t>(data_, data_.get() + n);
#endif
  size_ -= n;
  return *this;
}

auto replacement_map::value_type::pop_back(size_type n) -> value_type& {
  if (n > size_) throw std::overflow_error("replacement_map::value_type::pop_front");

  size_ -= n;
  return *this;
}

auto replacement_map::value_type::keep_front(size_type n) -> value_type& {
  if (n > size_) throw std::overflow_error("replacement_map::value_type::pop_front");

  size_ = n;
  return *this;
}

auto replacement_map::value_type::keep_back(size_type n) -> value_type& {
  if (n > size_) throw std::overflow_error("replacement_map::value_type::pop_front");
  const size_type advance = size_ - n;

  first += advance;
#if __cpp_lib_shared_ptr_arrays >= 201611
  data_ = std::shared_ptr<const std::uint8_t[]>(data_, data_.get() + advance);
#else
  data_ = std::shared_ptr<const std::uint8_t>(data_, data_.get() + advance);
#endif
  size_ = n;
  return *this;
}


replacement_map::tx::~tx() noexcept = default;

void replacement_map::tx::commit() noexcept {
  assert(map_ != nullptr);

  std::for_each(
      std::move_iterator(to_erase_.begin()),
      std::move_iterator(to_erase_.end()),
      [this](map_type::iterator&& iter) {
        map_->erase_and_dispose(iter, &replacement_map_dispose);
      });

  std::for_each(
      std::move_iterator(to_insert_.begin()),
      std::move_iterator(to_insert_.end()),
      [this](std::unique_ptr<value_type>&& entry) {
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
