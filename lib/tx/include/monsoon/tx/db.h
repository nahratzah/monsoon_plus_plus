#ifndef MONSOON_TX_DB_H
#define MONSOON_TX_DB_H

#include <monsoon/tx/detail/export_.h>
#include <monsoon/tx/txfile.h>
#include <monsoon/tx/detail/commit_id.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <cycle_ptr/cycle_ptr.h>

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
  static constexpr monsoon::io::fd::offset_type DB_OFF_END_ = DB_OFF_TX_ID_SEQ_ + detail::commit_manager::SIZE;

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

  public:
  auto begin(bool read_only) -> transaction;
  auto begin() const -> transaction;

  private:
  txfile f_; // Underlying file.
  std::shared_ptr<detail::commit_manager> cm_; // Allocate transaction IDs.
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
  monsoon_tx_local_ void commit_phase1(detail::commit_manager::write_id& tx);
  monsoon_tx_local_ void commit_phase2() noexcept;
  monsoon_tx_local_ auto validate() -> std::error_code;
  monsoon_tx_local_ void rollback() noexcept;

  virtual void do_commit_phase1(detail::commit_manager::write_id& tx) = 0;
  virtual void do_commit_phase2() noexcept = 0;
  virtual auto do_validate() -> std::error_code = 0;
  virtual void do_rollback() noexcept = 0;
};


/**
 * \brief Transaction inside the database.
 * \details
 * Holds on to all changes for a database.
 */
class monsoon_tx_export_ db::transaction {
  friend db;

  public:
  transaction(const transaction&) = delete;
  transaction& operator=(const transaction&) = delete;

  transaction() = default;
  transaction(transaction&&) noexcept;
  transaction& operator=(transaction&&) noexcept;
  ~transaction() noexcept;

  template<typename T>
  auto on(cycle_ptr::cycle_gptr<T> v) -> cycle_ptr::cycle_gptr<typename T::tx_object>;

  private:
  transaction(detail::commit_manager::commit_id seq, bool read_only, db& self) noexcept;

  public:
  auto seq() const noexcept -> detail::commit_manager::commit_id { return seq_; }
  auto before(const transaction& other) const noexcept -> bool;
  auto after(const transaction& other) const noexcept -> bool;

  void commit();
  void rollback() noexcept;

  auto active() const noexcept -> bool { return active_; }
  auto read_only() const noexcept -> bool { return read_only_; }
  auto read_write() const noexcept -> bool { return !read_only(); }

  private:
  detail::commit_manager::commit_id seq_;
  bool read_only_;
  bool active_ = false;
  std::unordered_map<cycle_ptr::cycle_gptr<const void>, cycle_ptr::cycle_gptr<transaction_obj>> callbacks_;
  db *self_;
};


} /* namespace monsoon::tx */

#include "db-inl.h"

#endif /* MONSOON_TX_DB_H */
