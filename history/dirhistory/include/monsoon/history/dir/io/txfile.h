#ifndef MONSOON_HISTORY_DIR_IO_TXFILE_H
#define MONSOON_HISTORY_DIR_IO_TXFILE_H

#include <utility>
#include <monsoon/io/fd.h>
#include <monsoon/history/dir/io/tx_sequencer.h>
#include <monsoon/history/dir/io/wal.h>

namespace monsoon::history::io {


class txfile {
  public:
  class transaction;

  txfile(monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len);
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
  transaction(const transaction&) = delete;
  transaction& operator=(const transaction&) = delete;

  transaction() = default;

  transaction(transaction&& x) noexcept
  : read_only_(x.read_only_),
    owner_(std::exchange(x.owner_, nullptr)),
    seq_(std::move(x.seq_))
  {}

  friend void swap(transaction& x, transaction& y) noexcept {
    using std::swap;
    swap(x.read_only_, y.read_only_);
    swap(x.owner_, y.owner_);
    swap(x.seq_, y.seq_);
  }

  transaction& operator=(transaction&& x) noexcept {
    if (*this) rollback();
    swap(*this, x);
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
