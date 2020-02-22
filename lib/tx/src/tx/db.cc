#include <monsoon/tx/db.h>
#include <monsoon/xdr/xdr.h>
#include <monsoon/xdr/xdr_stream.h>
#include <monsoon/io/limited_stream.h>
#include <monsoon/io/positional_stream.h>
#include <cassert>

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


} /* namespace monsoon::tx::<unnamed> */


db_invalid_error::~db_invalid_error() noexcept = default;


db::db(std::string name, monsoon::io::fd&& fd, monsoon::io::fd::offset_type off)
: db(name, validate_header_and_load_wal_(name, std::move(fd), off))
{}

db::db(std::string name, txfile&& f)
: f_(std::move(f)),
  tx_id_seq_(f_, DB_OFF_TX_ID_SEQ_)
{}

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
  sequence::init(init_tx, DB_OFF_TX_ID_SEQ_);
  init_tx.commit();

  return db(std::move(name), std::move(f));
}


db::transaction_obj::~transaction_obj() noexcept = default;

void db::transaction_obj::commit(txfile::transaction& tx) {
  do_commit(tx);
}

void db::transaction_obj::rollback() noexcept {
  do_rollback();
}


void db::transaction::commit() {
  if (!active_) return;
  assert(f_ != nullptr);

  auto tx = f_->begin(read_only_);
  for (auto& cb : callbacks_) cb->commit(tx);
  tx.commit();

  // Must be the last statement, so that exceptions will not mark this TX as done.
  active_ = false;
}

void db::transaction::rollback() noexcept {
  if (!active_) return;
  assert(f_ != nullptr);

  for (auto& cb : callbacks_) cb->rollback();
  active_ = false;
}


} /* namespace monsoon::tx */
