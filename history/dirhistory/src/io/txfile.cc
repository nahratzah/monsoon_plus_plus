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


void txfile::transaction::resize(size_type new_size) {
  if (!*this) throw txfile_bad_transaction("txfile::transaction::write_at");
  if (read_only_) throw txfile_read_only_transaction("txfile::transaction::write_at");

  wal_.resize(new_size);
}

auto txfile::transaction::write_at(offset_type off, const void* buf, std::size_t nbytes) -> std::size_t {
  if (!*this) throw txfile_bad_transaction("txfile::transaction::write_at");
  if (read_only_) throw txfile_read_only_transaction("txfile::transaction::write_at");

  wal_.write_at(off, buf, nbytes);
  return nbytes;
}

auto txfile::transaction::read_at(offset_type off, void* buf, std::size_t nbytes) const -> std::size_t {
  if (!*this) throw txfile_bad_transaction("txfile::transaction::read_at");

  return wal_.read_at(
      off, buf, nbytes,
      [this](offset_type off, void* buf, std::size_t& nbytes) -> std::size_t {
        // Read from the recorded change-sets of each transaction.
        return seq_.read_at(off, buf, nbytes);
      });
}

void txfile::transaction::commit() {
  if (!*this) throw txfile_bad_transaction("txfile::transaction::commit");

  if (!read_only_) {
    wal_.commit(
        [this](replacement_map&& undo_map) noexcept {
          seq_.commit(std::move(undo_map));
        });
  }

  owner_ = nullptr;
}

void txfile::transaction::rollback() noexcept {
  if (!*this) return;

  wal_.rollback();
  owner_ = nullptr;
}


} /* namespace monsoon::history::io */
