#ifndef MONSOON_HISTORY_DIR_IO_TXFILE_H
#define MONSOON_HISTORY_DIR_IO_TXFILE_H

#include <cstddef>
#include <utility>
#include <monsoon/io/fd.h>
#include <monsoon/history/dir/io/tx_sequencer.h>
#include <monsoon/history/dir/io/wal.h>

namespace monsoon::history::io {


class txfile {
  public:
  class transaction;

  txfile(const txfile&) = delete;
  txfile& operator=(const txfile&) = delete;

  txfile() {}
  txfile(monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len);

  txfile(txfile&&) noexcept = default;
  txfile& operator=(txfile&&) noexcept = default;

  ~txfile() noexcept;

  auto begin(bool read_only) -> transaction;
  auto begin() const -> transaction;

  private:
  monsoon::io::fd fd_;
  wal_region wal_;
  tx_sequencer sequencer_;
};


class txfile::transaction {
  friend txfile;

  public:
  using offset_type = monsoon::io::fd::offset_type;
  using size_type = monsoon::io::fd::size_type;

  transaction(const transaction&) = delete;
  transaction& operator=(const transaction&) = delete;

  friend void swap(transaction& x, transaction& y) noexcept {
    using std::swap;
    swap(x.read_only_, y.read_only_);
    swap(x.owner_, y.owner_);
    swap(x.seq_, y.seq_);
  }

  transaction() = default;

  transaction(transaction&& x) noexcept
  : read_only_(x.read_only_),
    owner_(std::exchange(x.owner_, nullptr)),
    seq_(std::move(x.seq_))
  {}

  transaction& operator=(transaction&& x) noexcept {
    if (*this) rollback();
    read_only_ = x.read_only_;
    owner_ = std::exchange(x.owner_, nullptr);
    seq_ = std::move(x.seq_);
    return *this;
  }

  private:
  explicit transaction(bool read_only, txfile& owner)
  : read_only_(read_only),
    owner_(&owner),
    seq_(owner.sequencer_.begin())
  {}

  public:
  ~transaction() noexcept {
    if (*this) rollback();
  }

  explicit operator bool() const noexcept {
    return owner_ != nullptr;
  }

  auto operator!() const noexcept -> bool {
    return !this->operator bool();
  }

  void commit();
  void rollback() noexcept;

  auto write_at(offset_type off, const void* buf, std::size_t nbytes) -> std::size_t;
  auto read_at(offset_type off, void* buf, std::size_t nbytes) const -> std::size_t;

  private:
  bool read_only_;
  txfile* owner_ = nullptr;
  tx_sequencer::tx seq_;
};


inline auto txfile::begin(bool read_only) -> transaction {
  return transaction(read_only, *this);
}

inline auto txfile::begin() const -> transaction {
  return const_cast<txfile&>(*this).begin(true);
}


} /* namespace monsoon::history::io */

#endif /* MONSOON_HISTORY_DIR_IO_TXFILE_H */
