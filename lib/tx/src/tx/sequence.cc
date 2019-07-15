#include <monsoon/tx/sequence.h>
#include <stdexcept>
#include <boost/endian/conversion.hpp>

namespace monsoon::tx {


sequence::sequence(txfile& f, monsoon::io::fd::offset_type off, type cache_size)
: txfile_impl_(f.pimpl_),
  off_(off),
  cache_size_(cache_size)
{
  if (cache_size <= 0) throw std::invalid_argument("sequence cache size must be at least 1");

  std::uint64_t magic;
  static_assert(sizeof(magic) == sizeof(MAGIC));

  auto tx = detail::wal_region::tx(std::shared_ptr<detail::wal_region>(f.pimpl_, &f.pimpl_->wal_));
  {
    auto r_off = off;
    std::uint8_t* r_buf = reinterpret_cast<std::uint8_t*>(&magic);
    auto r_len = sizeof(magic);
    while (r_len > 0) {
      const auto len = tx.read_at(r_off, r_buf, r_len);
      if (len == 0u) throw std::runtime_error("can't read sequence");
      r_off += len;
      r_buf += len;
      r_len -= len;
    }
  }
  boost::endian::big_to_native_inplace(magic);

  if (magic != MAGIC) throw std::runtime_error("sequence: incorrect magic");
}

auto sequence::operator()() -> type {
  std::lock_guard<std::mutex> lck{ mtx_ };

  if (cache_avail_ == 0) {
    // Start a read-write transaction.
    const auto owner = std::shared_ptr<txfile::impl_>(txfile_impl_);
    auto tx = detail::wal_region::tx(std::shared_ptr<detail::wal_region>(owner, &owner->wal_));

    // Read the value.
    {
      auto r_off = off_ + sizeof(MAGIC);
      std::uint8_t* r_buf = reinterpret_cast<std::uint8_t*>(&cache_val_);
      auto r_len = sizeof(cache_val_);
      while (r_len > 0) {
        const auto len = tx.read_at(r_off, r_buf, r_len);
        if (len == 0u) throw std::runtime_error("can't read sequence");
        r_off += len;
        r_buf += len;
        r_len -= len;
      }
    }
    boost::endian::big_to_native_inplace(cache_val_);

    // Increment and write back.
    auto write_back = cache_val_ + cache_size_;
    boost::endian::native_to_big_inplace(write_back);
    tx.write_at(off_ + sizeof(MAGIC), reinterpret_cast<const std::uint8_t*>(&write_back), sizeof(write_back));

    // Commit the allocation.
    tx.commit();

    // We now have some entries available.
    cache_avail_ = cache_size_;
  }


  --cache_avail_;
  return cache_val_++;
}


void sequence::init(txfile::transaction& tx, monsoon::io::fd::offset_type off, type init) {
  // Write the magic.
  auto magic = MAGIC;
  boost::endian::native_to_big_inplace(magic);
  {
    auto w_off = off;
    const std::uint8_t* w_buf = reinterpret_cast<const std::uint8_t*>(&magic);
    auto w_len = sizeof(magic);
    while (w_len > 0) {
      const auto len = tx.write_at(w_off, w_buf, w_len);
      if (len == 0u) throw std::runtime_error("can't write sequence");
      w_off += len;
      w_buf += len;
      w_len -= len;
    }
  }

  boost::endian::native_to_big_inplace(init);
  {
    auto w_off = off + sizeof(MAGIC);
    const std::uint8_t* w_buf = reinterpret_cast<const std::uint8_t*>(&init);
    auto w_len = sizeof(init);
    while (w_len > 0) {
      const auto len = tx.write_at(w_off, w_buf, w_len);
      if (len == 0u) throw std::runtime_error("can't write sequence");
      w_off += len;
      w_buf += len;
      w_len -= len;
    }
  }
}


} /* namespace monsoon::tx */
