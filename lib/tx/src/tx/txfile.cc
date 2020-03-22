#include <monsoon/tx/txfile.h>
#include <monsoon/tx/sequence.h>
#include <mutex>
#include <type_traits>

namespace monsoon::tx {


txfile_transaction_error::~txfile_transaction_error() = default;
txfile_bad_transaction::~txfile_bad_transaction() = default;
txfile_read_only_transaction::~txfile_read_only_transaction() = default;


txfile::impl_::~impl_() noexcept = default;


txfile::txfile(std::string name, monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len)
: pimpl_(std::make_shared<impl_>(std::move(name), std::move(fd), off, len))
{}

txfile::txfile(std::string name, create_tag tag, monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len)
: pimpl_(std::make_shared<impl_>(std::move(name), tag, std::move(fd), off, len))
{}

auto txfile::create(std::string name, monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len) -> txfile {
  return txfile(std::move(name), create_tag(), std::move(fd), off, len);
}


template<typename CB>
txfile::transaction::transaction(bool read_only, const std::shared_ptr<impl_>& owner, CB&& cb)
: read_only_(read_only),
  owner_(owner.get()),
  seq_(std::shared_ptr<detail::tx_sequencer>(owner, &owner->sequencer_), cb),
  wal_(std::shared_ptr<detail::wal_region>(owner, &owner->wal_))
{}

void txfile::transaction::resize(size_type new_size) {
  if (!*this) throw txfile_bad_transaction("txfile::transaction::resize");
  if (read_only_) throw txfile_read_only_transaction("txfile::transaction::resize");

  wal_.resize(new_size);
}

auto txfile::transaction::size() const -> size_type {
  if (!*this) throw txfile_bad_transaction("txfile::transaction::resize");

  return wal_.size();
}

auto txfile::transaction::write_at(offset_type off, const void* buf, std::size_t nbytes) -> std::size_t {
  if (!*this) throw txfile_bad_transaction("txfile::transaction::write_at");
  if (read_only_) throw txfile_read_only_transaction("txfile::transaction::write_at");

  wal_.write_at(off, buf, nbytes);
  return nbytes;
}

void txfile::transaction::write_at_many(std::vector<offset_type> off, const void* buf, std::size_t nbytes) {
  if (!*this) throw txfile_bad_transaction("txfile::transaction::write_at_many");
  if (read_only_) throw txfile_read_only_transaction("txfile::transaction::write_at_many");

  wal_.write_at(std::move(off), buf, nbytes);
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
        [this](detail::replacement_map&& undo_map) noexcept {
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


static_assert(std::is_same_v<txfile::id_type, sequence::type>);


auto txfile::begin(bool read_only) -> transaction {
  return transaction(read_only, pimpl_, []() {});
}

auto txfile::begin(sequence& s, bool read_only) -> std::tuple<transaction, id_type> {
  id_type tx_id;
  auto tx = transaction(
      read_only,
      pimpl_,
      [&tx_id, &s]() {
        tx_id = s();
      });
  return std::make_tuple(std::move(tx), std::move(tx_id));
}

auto txfile::begin() const -> transaction {
  return transaction(true, pimpl_, []() {});
}

auto txfile::begin(sequence& s) const -> std::tuple<transaction, id_type> {
  id_type tx_id;
  auto tx = transaction(
      true,
      pimpl_,
      [&tx_id, &s]() {
        tx_id = s();
      });
  return std::make_tuple(std::move(tx), std::move(tx_id));
}


} /* namespace monsoon::tx */
