#include <monsoon/tx/detail/replacement_map.h>
#include <monsoon/io/rw.h>
#include <memory>
#include <stdexcept>

namespace monsoon::tx::detail {
namespace {


auto replacement_map_clone(const replacement_map::value_type& r) -> replacement_map::value_type* {
  return new replacement_map::value_type(r);
}

void replacement_map_dispose(replacement_map::value_type* ptr) {
  delete ptr;
}


}


class replacement_map::reader_intf_ {
  public:
  virtual ~reader_intf_() noexcept;

  virtual void read(void* buf, std::size_t len) = 0;
  virtual auto size() const noexcept -> std::size_t = 0;
  virtual void advance(std::size_t n) = 0;

  auto operator+=(std::size_t n) -> reader_intf_& {
    advance(n);
    return *this;
  }
};

class replacement_map::buf_reader_
: public reader_intf_
{
  public:
  buf_reader_(const void* buf, std::size_t len) noexcept
  : buf_(buf),
    len_(len)
  {}

  ~buf_reader_() noexcept override;

  void read(void* buf, std::size_t len) override {
    assert(len <= len_);
    std::memcpy(buf, buf_, len);
  }

  auto size() const noexcept -> std::size_t override {
    return len_;
  }

  void advance(std::size_t n) override {
    assert(n <= len_);
    buf_ = reinterpret_cast<const std::uint8_t*>(buf_) + n;
    len_ -= n;
  }

  private:
  const void* buf_;
  std::size_t len_;
};

class replacement_map::fd_reader_
: public reader_intf_
{
  public:
  fd_reader_(const monsoon::io::fd& f, monsoon::io::fd::offset_type off, std::size_t len) noexcept
  : f_(f),
    off_(off),
    len_(len)
  {}

  ~fd_reader_() noexcept override;

  void read(void* buf, std::size_t len) override {
    assert(len <= len_);
    if (off_ >= f_.size()) {
      std::memset(buf, 0, len);
    } else {
      if (f_.size() - off_ < len) {
        const std::size_t rlen = f_.size() - off_;
        std::memset(reinterpret_cast<std::uint8_t*>(buf) + rlen, 0, len - rlen);
        len = rlen;
      }

      f_.read_at(off_, buf, len);
    }
  }

  auto size() const noexcept -> std::size_t override {
    return len_;
  }

  void advance(std::size_t n) override {
    assert(n <= len_);
    off_ += n;
    len_ -= n;
  }

  private:
  const monsoon::io::fd& f_;
  monsoon::io::fd::offset_type off_;
  std::size_t len_;
};

class replacement_map::aio_reader_
: public reader_intf_
{
  public:
  aio_reader_(monsoon::io::aio::const_fd_target f, monsoon::io::fd::offset_type off, std::size_t len) noexcept
  : f_(f),
    off_(off),
    len_(len)
  {}

  ~aio_reader_() noexcept override;

  void read(void* buf, std::size_t len) override {
    assert(len <= len_);
    if (off_ >= f_.filesize()) {
      std::memset(buf, 0, len);
    } else {
      if (f_.filesize() - off_ < len) {
        const std::size_t rlen = f_.filesize() - off_;
        std::memset(reinterpret_cast<std::uint8_t*>(buf) + rlen, 0, len - rlen);
        len = rlen;
      }

      f_.read_at(off_, buf, len);
    }
  }

  auto size() const noexcept -> std::size_t override {
    return len_;
  }

  void advance(std::size_t n) override {
    assert(n <= len_);
    off_ += n;
    len_ -= n;
  }

  private:
  monsoon::io::aio::const_fd_target f_;
  monsoon::io::fd::offset_type off_;
  std::size_t len_;
};


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
  y.map_.clear_and_dispose(&replacement_map_dispose);
  return *this;
}

replacement_map::~replacement_map() noexcept {
  map_.clear_and_dispose(&replacement_map_dispose);
}

void replacement_map::clear() noexcept {
  map_.clear_and_dispose(&replacement_map_dispose);
}

void replacement_map::truncate(monsoon::io::fd::size_type new_size) noexcept {
  // Find the first region *after* new_size.
  auto iter = map_.lower_bound(new_size);
  assert(iter == map_.end() || iter->begin_offset() >= new_size);
  assert(iter == map_.begin() || std::prev(iter)->begin_offset() < new_size);

  map_.erase_and_dispose(iter, map_.end(), &replacement_map_dispose);

  if (!map_.empty()) {
    auto& back = *map_.rbegin();
    if (back.end_offset() > new_size)
      back.keep_front(new_size - back.begin_offset());
  }
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
    if (iter != map_.end() && nbytes > iter->begin_offset() - off)
      nbytes = iter->begin_offset() - off;
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
  auto r = buf_reader_(buf, nbytes);
  return write_at_(off, r, overwrite);
}

auto replacement_map::write_at_from_file(monsoon::io::fd::offset_type off, const monsoon::io::fd& fd, monsoon::io::fd::offset_type fd_off, std::size_t nbytes, bool overwrite) -> tx {
  auto r = fd_reader_(fd, fd_off, nbytes);
  return write_at_(off, r, overwrite);
}

auto replacement_map::write_at_from_file(monsoon::io::fd::offset_type off, monsoon::io::aio::const_fd_target fd, monsoon::io::fd::offset_type fd_off, std::size_t nbytes, bool overwrite) -> tx {
  auto r = aio_reader_(std::move(fd), fd_off, nbytes);
  return write_at_(off, r, overwrite);
}

auto replacement_map::write_at_(monsoon::io::fd::offset_type off, reader_intf_& r, bool overwrite) -> tx {
  if (overwrite)
    return write_at_with_overwrite_(off, r);
  else
    return write_at_without_overwrite_(off, r);
}

auto replacement_map::write_at_with_overwrite_(monsoon::io::fd::offset_type off, reader_intf_& r) -> tx {
  const monsoon::io::fd::offset_type end_off = off + r.size();
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
  while (r.size() > 0) {
    std::unique_ptr<std::uint8_t[]> vector;
    std::size_t to_reserve = r.size();

    for (;;) {
      try {
        vector = std::make_unique<std::uint8_t[]>(to_reserve);
        break;
      } catch (...) {
        if (to_reserve <= 1) throw;
        to_reserve /= 2;
      }
    }

    r.read(vector.get(), to_reserve);
    r += to_reserve;

    t.to_insert_.emplace_back(std::make_unique<value_type>(off, std::move(vector), to_reserve));
    off += to_reserve;
  }

  return t;
}

auto replacement_map::write_at_without_overwrite_(monsoon::io::fd::offset_type off, reader_intf_& r) -> tx {
  const monsoon::io::fd::offset_type end_off = off + r.size();
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
      r += skip;
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

      r.read(vector.get(), to_reserve);
      t.to_insert_.emplace_back(std::make_unique<value_type>(off, std::move(vector), to_reserve));
      off += to_reserve;
      r += to_reserve;
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


replacement_map::reader_intf_::~reader_intf_() noexcept = default;
replacement_map::buf_reader_::~buf_reader_() noexcept = default;
replacement_map::fd_reader_::~fd_reader_() noexcept = default;
replacement_map::aio_reader_::~aio_reader_() noexcept = default;


} /* namespace monsoon::tx::detail */
