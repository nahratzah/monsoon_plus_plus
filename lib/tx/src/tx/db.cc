#include <monsoon/tx/db.h>
#include <monsoon/tx/db_errc.h>
#include <monsoon/tx/tx_aware_data.h>
#include <monsoon/xdr/xdr.h>
#include <monsoon/xdr/xdr_stream.h>
#include <monsoon/io/limited_stream.h>
#include <monsoon/io/positional_stream.h>
#include <monsoon/io/rw.h>
#include <boost/endian/conversion.hpp>
#include <algorithm>
#include <cassert>
#include <iterator>
#include <limits>

namespace monsoon::tx {
namespace {


/**
 * \brief The header in front of the WAL of the database.
 * \details
 * Validates the database mime-magic, and maintains information on the size of the WAL.
 *
 * Note: this datatype can never be changed, because it is unversioned.
 * When making changes, instead change the types inside the database, which are
 * transactional (and thus can be upgraded atomically).
 */
struct front_header {
  static constexpr std::size_t SIZE = 24;

  static constexpr std::array<std::uint8_t, 15> MAGIC = {{
    17u, 19u,  7u, 11u,
    'M', 'O', 'N', '-',
    's', 'o', 'o', 'n',
    '-', 'd', 'b'
  }};

  std::uint64_t wal_bytes;

  void read(const monsoon::io::fd& f, monsoon::io::fd::offset_type off);
  void read(monsoon::xdr::xdr_istream& i);
  void write(monsoon::io::fd& f, monsoon::io::fd::offset_type off);
  void write(monsoon::xdr::xdr_ostream& o);
};

void front_header::read(const monsoon::io::fd& f, monsoon::io::fd::offset_type off) {
  using segment = monsoon::io::limited_stream_reader<monsoon::io::positional_reader>;

  auto x = monsoon::xdr::xdr_stream_reader<segment>(segment(SIZE, f, off));
  read(x);
  assert(x.at_end());
}

void front_header::read(monsoon::xdr::xdr_istream& i) {
  std::array<std::uint8_t, MAGIC.size()> magic;
  i.get_array(magic);
  if (magic != MAGIC) throw db_invalid_error("magic mismatch");

  wal_bytes = i.get_uint64();
}

void front_header::write(monsoon::io::fd& f, monsoon::io::fd::offset_type off) {
  using segment = monsoon::io::limited_stream_writer<monsoon::io::positional_writer>;

  auto x = monsoon::xdr::xdr_stream_writer<segment>(segment(SIZE, f, off));
  write(x);
}

void front_header::write(monsoon::xdr::xdr_ostream& o) {
  o.put_array(MAGIC);
  o.put_uint64(wal_bytes);
}


template<typename R>
void read_all_into_buf(R& r, monsoon::io::fd::offset_type off, void* buf, std::size_t len) {
  auto byte_buf = reinterpret_cast<std::uint8_t*>(buf);

  while (len > 0) {
    const auto rlen = r.read_at(off, byte_buf, len);
    if (rlen == 0) throw std::runtime_error("unable to read");
    byte_buf += rlen;
    len -= rlen;
    off += rlen;
  }
}

template<typename W>
void write_all_from_buf(W& w, monsoon::io::fd::offset_type off, const void* buf, std::size_t len) {
  auto byte_buf = reinterpret_cast<const std::uint8_t*>(buf);

  while (len > 0) {
    const auto wlen = w.write_at(off, byte_buf, len);
    if (wlen == 0) throw std::runtime_error("unable to read");
    byte_buf += wlen;
    len -= wlen;
    off += wlen;
  }
}


} /* namespace monsoon::tx::<unnamed> */


db_invalid_error::~db_invalid_error() noexcept = default;


db::db(std::string name, monsoon::io::fd&& fd, monsoon::io::fd::offset_type off)
: db(name, validate_header_and_load_wal_(name, std::move(fd), off))
{}

db::db(std::string name, txfile&& f)
: f_(std::move(f)),
  cm_(detail::commit_manager::allocate(f_, DB_OFF_TX_ID_SEQ_))
{
  std::uint32_t version;

  // Validate version.
  auto t = f_.begin(true);
  read_all_into_buf(t, DB_OFF_VERSION_, &version, sizeof(version));
  boost::endian::big_to_native_inplace(version);
  if (version > VERSION) throw db_invalid_error("unsupported database version");
  t.commit();
}

auto db::validate_header_and_load_wal_(const std::string& name, monsoon::io::fd&& fd, monsoon::io::fd::offset_type off) -> txfile {
  front_header fh;
  fh.read(fd, off);

  return txfile(name, std::move(fd), off + front_header::SIZE, fh.wal_bytes);
}

auto db::create(std::string name, monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type wal_len) -> db {
  if (wal_len > 0xffff'ffff'ffff'ffffULL)
    throw std::invalid_argument("wal size must be a 64-bit integer");

  front_header fh;
  fh.wal_bytes = wal_len;
  fh.write(fd, off);

  auto f = txfile::create(name, std::move(fd), off + front_header::SIZE, wal_len);

  // Build up static database elements.
  auto init_tx = f.begin();
  init_tx.resize(DB_HEADER_SIZE);
  // Write the version.
  const std::uint32_t version = boost::endian::big_to_native(VERSION);
  write_all_from_buf(init_tx, DB_OFF_VERSION_, &version, sizeof(version));
  // Initialize transaction sequence.
  detail::commit_manager::init(init_tx, DB_OFF_TX_ID_SEQ_);
  // Commit all written data.
  init_tx.commit();

  return db(std::move(name), std::move(f));
}

auto db::begin(bool read_only) -> transaction {
  return transaction(cm_->get_tx_commit_id(), read_only, *this);
}

auto db::begin() const -> transaction {
  return transaction(cm_->get_tx_commit_id(), true, const_cast<db&>(*this));
}


db::transaction_obj::~transaction_obj() noexcept = default;

void db::transaction_obj::do_commit_phase1(detail::commit_manager::write_id& tx) {}
void db::transaction_obj::do_commit_phase2(const detail::commit_manager::commit_id& write_id) noexcept {}
auto db::transaction_obj::do_validate(const detail::commit_manager::commit_id& write_id) -> std::error_code { return {}; }
void db::transaction_obj::do_rollback() noexcept {}

void db::transaction_obj::commit_phase1(detail::commit_manager::write_id& tx) {
  do_commit_phase1(tx);
}

void db::transaction_obj::commit_phase2(const detail::commit_manager::commit_id& write_id) noexcept {
  do_commit_phase2(write_id);
}

auto db::transaction_obj::validate(const detail::commit_manager::commit_id& write_id) -> std::error_code {
  return do_validate(write_id);
}

void db::transaction_obj::rollback() noexcept {
  do_rollback();
}


auto db::transaction::visible(const cycle_ptr::cycle_gptr<tx_aware_data>& datum) const noexcept -> bool {
  assert(datum != nullptr);
  if (deleted_set_.count(datum) > 0) return false;
  if (created_set_.count(datum) > 0) return true;
  return datum->visible_in_tx(seq_);
}

void db::transaction::commit() {
  if (!active_) throw std::logic_error("commit called on inactive transaction");
  assert(self_ != nullptr);

  if (read_only_) {
    rollback_();
  } else {
    auto tx = self_->cm_->prepare_commit(self_->f_);
    const auto layout_locks = lock_all_layouts_();

    commit_phase1_(tx);
    tx.apply(
        [this, &tx]() -> std::error_code {
          return validate_(tx.seq());
        },
        [this, &tx]() noexcept {
          commit_phase2_(tx.seq());
        });
  }

  // Must be the last statement, so that exceptions will not mark this TX as done.
  active_ = false;
}

void db::transaction::rollback() noexcept {
  if (!active_) return;
  assert(self_ != nullptr);

  rollback_();
  active_ = false;
}

auto db::transaction::lock_all_layouts_() const -> std::vector<std::shared_lock<std::shared_mutex>> {
  // Build up the layout locks.
  // There are multiple layouts, and we must avoid deadlock.
  std::vector<std::shared_lock<std::shared_mutex>> layout_locks;
  layout_locks.reserve(callbacks_.size());

  // Create unlocked list in arbitrary order.
  std::transform(
      callbacks_.begin(), callbacks_.end(),
      std::back_inserter(layout_locks),
      [](const auto& pair) -> std::shared_lock<std::shared_mutex> {
        return std::shared_lock<std::shared_mutex>(
            pair.second->layout_lck(),
            std::defer_lock);
      });

  // To make a deterministic lock order, we use the memory address of the mutex
  // (which is a proxy for the memory address of the transaction_obj).
  std::sort(
      layout_locks.begin(), layout_locks.end(),
      [](const std::shared_lock<std::shared_mutex>& x, const std::shared_lock<std::shared_mutex>& y) noexcept {
        return x.mutex() < y.mutex();
      });

  // Now that we have all locks prepared and ordered deterministically, we can acquire the lock.
  std::for_each(
      layout_locks.begin(), layout_locks.end(),
      [](std::shared_lock<std::shared_mutex>& lck) {
        lck.lock();
      });

  return layout_locks;
}

void db::transaction::commit_phase1_(detail::commit_manager::write_id& tx) {
  // Write all creation records.
  // XXX create a bundle-write in the WAL where we can use 1 buffer and many offsets.
  std::for_each(
      created_set_.begin(), created_set_.end(),
      [&tx, buf=tx_aware_data::make_creation_buffer(tx.seq().val())](const auto& datum_ptr) {
        monsoon::io::write_at(tx, datum_ptr->offset() + tx_aware_data::CREATION_OFFSET, buf.data(), buf.size());
      });

  // Write all deletion records.
  // XXX create a bundle-write in the WAL where we can use 1 buffer and many offsets.
  std::for_each(
      created_set_.begin(), created_set_.end(),
      [&tx, buf=tx_aware_data::make_deletion_buffer(tx.seq().val())](const auto& datum_ptr) {
        monsoon::io::write_at(tx, datum_ptr->offset() + tx_aware_data::DELETION_OFFSET, buf.data(), buf.size());
      });

  // Run phase1 on each transaction object.
  std::for_each(
      callbacks_.begin(), callbacks_.end(),
      [&tx](auto& cb) {
        cb.second->commit_phase1(tx);
      });
}

void db::transaction::commit_phase2_(const detail::commit_manager::commit_id& write_id) noexcept {
  // In-memory: mark all created objects.
  std::for_each(
      created_set_.begin(), created_set_.end(),
      [&write_id](const auto& datum_ptr) {
        datum_ptr->set_created(write_id.val());
      });

  // In-memory: mark all deleted objects.
  std::for_each(
      deleted_set_.begin(), deleted_set_.end(),
      [&write_id](const auto& datum_ptr) {
        datum_ptr->set_deleted(write_id.val());
      });

  // Run phase2 on each transaction object.
  std::for_each(
      callbacks_.begin(), callbacks_.end(),
      [&write_id](auto& cb) {
        cb.second->commit_phase2(write_id);
      });
}

auto db::transaction::validate_(const detail::commit_manager::commit_id& write_id) -> std::error_code {
  // Validate all required objects are still here.
  for (const auto& datum : require_set_) {
    if (deleted_set_.count(datum) > 0)
      return db_errc::deleted_required_object_in_tx;
    if (created_set_.count(datum) == 0
        && !datum->visible_in_tx(write_id))
      return db_errc::deleted_required_object;
  }

  // Validate deleted objects are present (avoid double deletes).
  for (const auto& datum : deleted_set_) {
    if (created_set_.count(datum) == 0
        && !datum->visible_in_tx(write_id))
      return db_errc::double_delete;
  }

  // Run callback validations.
  for (auto& cb : callbacks_) {
    std::error_code ec = cb.second->validate(write_id);
    if (ec) return ec;
  }
  return {};
}

void db::transaction::rollback_() noexcept {
  std::for_each(
      callbacks_.begin(), callbacks_.end(),
      [](auto& cb) {
        cb.second->rollback();
      });
}


} /* namespace monsoon::tx */
