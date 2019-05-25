#include <monsoon/history/dir/io/replacement_map.h>

namespace monsoon::history::io {


replacement_map::~replacement_map() noexcept {
  map_.clear_and_dispose([](entry_type* ptr) { delete ptr; });
}

auto replacement_map::read_at(monsoon::io::fd::offset_type off, void* buf, std::size_t& nbytes) const -> std::size_t {
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

auto replacement_map::write_at(monsoon::io::fd::offset_type off, const void* buf, std::size_t nbytes) -> tx {
  const monsoon::io::fd::offset_type end_off = off + nbytes;
  if (end_off < off) throw std::overflow_error("replacement_map: off + nbytes");

  tx t;
  t.map_ = &map_;
  std::size_t bytes_from_pred = 0, bytes_from_succ = 0;

  // Find the first region *after* off.
  map_type::iterator succ = map_.upper_bound(off);
  assert(succ == map_.end() || succ->first > off);

  // Find the region at-or-before off.
  const map_type::iterator pred = (succ == map_.begin() ? map_.end() : std::prev(succ));
  assert(pred == map_.end() || pred->first <= off);

  // We may need to include some bytes from pred.
  if (pred != map_.end() && pred->end_offset() > off) {
    t.to_erase_.push_back(pred);
    bytes_from_pred = off - pred->first;
  }

  // Mark any full-replaced entries as to-be-erased.
  // Move succ forward until it's the first not-fully-replaced entry after off.
  while (succ != map_.end() && succ->end_offset() <= end_off)
    t.to_erase_.push_back(succ++);

  // We may need to include bytes from succ.
  if (succ != map_.end() && succ->first < end_off) {
    t.to_erase_.push_back(succ);
    bytes_from_succ = succ->end_offset() - end_off;
  }

  // Reserve at least some bytes into the vector.
  vector_type vector;
  const std::uint8_t* pred_buf = pred->second.data();
  const std::uint8_t* byte_buf = reinterpret_cast<const std::uint8_t*>(buf);
  const std::uint8_t* succ_buf = succ->second.data() + succ->second.size() - bytes_from_succ;
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
      to_reserve += nbytes;

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
    vector_pos = std::copy_n(byte_buf, wlen, vector_pos);
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
        assert(ipos == map_->begin() || std::prev(ipos)->end_offset() <= ipos->first);
        assert(std::next(ipos) == map_->end() || std::next(ipos)->first >= ipos->end_offset());
      });

  map_ = nullptr;
  to_erase_.clear();
  to_insert_.clear();
}


} /* namespace monsoon::history::io */
