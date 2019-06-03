#include <monsoon/history/dir/io/txfile.h>
#include <mutex>

namespace monsoon::history::io {


txfile_transaction_error::~txfile_transaction_error() = default;
txfile_bad_transaction::~txfile_bad_transaction() = default;
txfile_read_only_transaction::~txfile_read_only_transaction() = default;


txfile::impl_::~impl_() noexcept = default;


txfile::txfile(monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len)
: pimpl_(std::make_shared<impl_>(std::move(fd), off, len))
{}

txfile::txfile(create_tag tag, monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len)
: pimpl_(std::make_shared<impl_>(tag, std::move(fd), off, len))
{}

auto txfile::create(monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len) -> txfile {
  return txfile(create_tag(), std::move(fd), off, len);
}


auto txfile::transaction::write_at(offset_type off, const void* buf, std::size_t nbytes) -> std::size_t {
  if (!*this) throw txfile_bad_transaction("txfile::transaction::write_at");
  if (read_only_) throw txfile_read_only_transaction("txfile::transaction::write_at");

  wal_.write_at(off, buf, nbytes);
  return nbytes;
}

auto txfile::transaction::read_at(offset_type off, void* buf, std::size_t nbytes) const -> std::size_t {
  if (!*this) throw txfile_bad_transaction("txfile::transaction::read_at");

  // Protect against changes in the transaction sequence.
  std::shared_lock lck{ owner_->mtx_ };

  return wal_.read_at(
      off, buf, nbytes,
      [this, &lck](offset_type off, void* buf, std::size_t& nbytes) -> std::size_t {
        // Read from the recorded change-sets of each transaction.
        const auto rlen = seq_.read_at(off, buf, nbytes);
        lck.unlock();
        return rlen;
      });
}

void txfile::transaction::commit() {
  if (!*this) throw txfile_bad_transaction("txfile::transaction::commit");

  if (!read_only_) {
    std::unique_lock lck{ owner_->mtx_ };

    seq_.record() = wal_.commit();
    seq_.commit();
  }

  owner_ = nullptr;
}


} /* namespace monsoon::history::io */
