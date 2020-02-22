#ifndef MONSOON_TX_DB_H
#define MONSOON_TX_DB_H

#include <monsoon/tx/detail/export_.h>
#include <monsoon/tx/txfile.h>
#include <monsoon/tx/sequence.h>
#include <cstdint>
#include <memory>
#include <vector>

namespace monsoon::tx {


class monsoon_tx_export_ db_invalid_error : public std::runtime_error {
  public:
  using std::runtime_error::runtime_error;
  ~db_invalid_error() noexcept override;
};


class monsoon_tx_export_ db {
  public:
  static constexpr std::uint32_t VERSION = 0;
  static constexpr monsoon::io::fd::size_type DEFAULT_WAL_BYTES = 32 * 1024 * 1024;

  // Database header will be exactly this size in bytes. There may be unused space.
  static constexpr monsoon::io::fd::offset_type DB_HEADER_SIZE = 4096;

  private:
  // Offset: version number (4 bytes)
  static constexpr monsoon::io::fd::offset_type DB_OFF_VERSION_ = 0;
  // Offset: tx_id_seq
  static constexpr monsoon::io::fd::offset_type DB_OFF_TX_ID_SEQ_ = DB_OFF_VERSION_ + 4u;
  // End of used space.
  static constexpr monsoon::io::fd::offset_type DB_OFF_END_ = DB_OFF_TX_ID_SEQ_ + sequence::SIZE;

  static_assert(DB_OFF_END_ <= DB_HEADER_SIZE, "db header should fit in reserved space");

  public:
  class transaction;
  class transaction_obj;
  class db_obj;

  db(const db&) = delete;
  db& operator=(const db&) = delete;

  db() = default;
  db(db&&) noexcept = default;
  db& operator=(db&&) noexcept = default;

  /**
   * \brief Open an existing database.
   * \details Recovers the file.
   * \param[in] name The name under which instrumentation is to be published.
   * \param[in] fd The file descriptor of the file.
   * \param[in] off The offset at which the DB is found.
   */
  db(std::string name, monsoon::io::fd&& fd, monsoon::io::fd::offset_type off = 0);

  /**
   * \brief Initialize a database.
   * \details Initializes the txfile to an empty file.
   * \param[in] name The name under which instrumentation is to be published.
   * \param[in] fd The file descriptor of the file.
   * \param[in] off The offset at which the DB is found.
   * \param[in] wal_len The length in bytes of the WAL.
   */
  static auto create(std::string name, monsoon::io::fd&& fd, monsoon::io::fd::offset_type off = 0, monsoon::io::fd::size_type len = DEFAULT_WAL_BYTES) -> db;

  private:
  monsoon_tx_local_ db(std::string name, txfile&& f);

  ///\brief Validate the header in front of the WAL and uses it to load the WAL.
  monsoon_tx_local_ auto validate_header_and_load_wal_(const std::string& name, monsoon::io::fd&& fd, monsoon::io::fd::offset_type off) -> txfile;

  private:
  txfile f_; // Underlying file.
  sequence tx_id_seq_; // Allocate transaction IDs.
};


/**
 * \brief Transaction participant.
 * \details
 * Interface for specific types that participate in a transaction.
 */
class monsoon_tx_export_ db::transaction_obj {
  friend transaction;

  public:
  virtual ~transaction_obj() noexcept;

  private:
  monsoon_tx_local_ void commit(txfile::transaction& tx);
  monsoon_tx_local_ void rollback() noexcept;

  virtual void do_commit(txfile::transaction& tx) = 0;
  virtual void do_rollback() noexcept = 0;
};


/**
 * \brief Transaction inside the database.
 * \details
 * Holds on to all changes for a database.
 */
class monsoon_tx_export_ db::transaction {
  public:
  using seq_type = std::uintmax_t;

  transaction(const transaction&) = delete;
  transaction& operator=(const transaction&) = delete;

  transaction() = default;
  transaction(transaction&&) noexcept;
  transaction& operator=(transaction&&) noexcept;
  ~transaction() noexcept;

  auto seq() const noexcept -> seq_type { return seq_; }

  static auto before(seq_type x, seq_type y) noexcept -> bool;
  auto before(const transaction& other) const noexcept -> bool;
  auto after(const transaction& other) const noexcept -> bool;

  void commit();
  void rollback() noexcept;

  auto active() const noexcept -> bool { return active_; }
  auto read_only() const noexcept -> bool { return read_only_; }
  auto read_write() const noexcept -> bool { return !read_only(); }

  private:
  seq_type seq_;
  bool read_only_;
  bool active_ = false;
  std::vector<std::unique_ptr<transaction_obj>> callbacks_;
  txfile *f_;
};


} /* namespace monsoon::tx */

#include "db-inl.h"

#endif /* MONSOON_TX_DB_H */
