#include <monsoon/history/dir/io/txfile.h>

namespace monsoon::history::io {


txfile_transaction_error::~txfile_transaction_error() = default;
txfile_bad_transaction::~txfile_bad_transaction() = default;
txfile_read_only_transaction::~txfile_read_only_transaction() = default;


txfile::impl_::~impl_() noexcept = default;


txfile::txfile(monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len)
: pimpl_(std::make_unique<impl_>(std::move(fd), off, len))
{}

txfile::txfile(create_tag tag, monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len)
: pimpl_(std::make_unique<impl_>(tag, std::move(fd), off, len))
{}

auto txfile::create(monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len) -> txfile {
  return txfile(create_tag(), std::move(fd), off, len);
}


auto txfile::transaction::write_at(offset_type off, const void* buf, std::size_t nbytes) -> std::size_t {
  if (!*this) throw txfile_bad_transaction("txfile::transaction::write_at");
  if (read_only_) throw txfile_read_only_transaction("txfile::transaction::write_at");

  auto replacements_tx = replacements_.write_at(off, buf, nbytes);

#if 0 // XXX implement required code
  // We do a late allocation of a WAL ID, to minimize pressure.
  if (!tx_id_.has_value()) tx_id_.emplace(wal_.allocate_tx_id());
#endif

  assert(tx_id_.has_value());
  auto wal_op = wal_record::make_write(
      *tx_id_,
      offset_to_fd_offset_(off),
      std::vector<std::uint8_t>(reinterpret_cast<const std::uint8_t*>(buf), reinterpret_cast<const std::uint8_t*>(buf) + nbytes));

#if 0 // XXX implement required code
  // Record the operation in the WAL.
  owner_->wal_.append(std::move(wal_op));
#endif

  replacements_tx.commit();

  return nbytes;
}

auto txfile::transaction::read_at(offset_type off, void* buf, std::size_t nbytes) const -> std::size_t {
  if (!*this) throw txfile_bad_transaction("txfile::transaction::read_at");

  // Read from the writes in the transaction.
  std::size_t rlen = replacements_.read_at(off, buf, nbytes);
  if (rlen != 0u) return rlen;
  // nbytes will have been modified to not overlap any replacements_.

  // Protect against changes in the file contents and transaction sequence.
  std::shared_lock lck{ owner_->mtx_ };

  // Read from the recorded change-sets of each transaction.
  rlen = seq_.read_at(off, buf, nbytes);
  if (rlen != 0u) return rlen;
  // nbytes will have been modified to not overlap any transactions that apply.

  // data is only present in the file.
  return owner_->fd_.read_at(offset_to_fd_offset_(off), buf, nbytes);
}


} /* namespace monsoon::history::io */
