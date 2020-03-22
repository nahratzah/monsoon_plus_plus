#ifndef MONSOON_TX_DB_H
#define MONSOON_TX_DB_H

#include <monsoon/tx/detail/export_.h>
#include <monsoon/tx/txfile.h>
#include <monsoon/tx/detail/commit_manager.h>
#include <monsoon/tx/detail/db_cache.h>
#include <monsoon/tx/detail/layout_domain.h>
#include <monsoon/shared_resource_allocator.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cycle_ptr/cycle_ptr.h>

namespace monsoon::tx {


class tx_aware_data;


class monsoon_tx_export_ db_invalid_error : public std::runtime_error {
  public:
  using std::runtime_error::runtime_error;
  ~db_invalid_error() noexcept override;
};


struct db_options {
  std::uintptr_t max_memory = detail::db_cache::default_max_memory;
  shared_resource_allocator<std::byte> allocator;
};


class monsoon_tx_export_ db
: public std::enable_shared_from_this<db>
{
  public:
  static constexpr std::uint32_t VERSION = 1;
  static constexpr monsoon::io::fd::size_type DEFAULT_WAL_BYTES = 32 * 1024 * 1024;

  // Database header will be exactly this size in bytes. There may be unused space.
  // Note that this value can never be changed, because it is used in file encoding.
  static constexpr monsoon::io::fd::offset_type DB_HEADER_SIZE = 4096;

  private:
  // Offset: version number (4 bytes)
  static constexpr monsoon::io::fd::offset_type DB_OFF_VERSION_ = 0;
  // Offset: tx_id_seq (present when VERSION = 1)
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
  db(std::string name, monsoon::io::fd&& fd, monsoon::io::fd::offset_type off = 0, const db_options& options = db_options());

  /**
   * \brief Initialize a database.
   * \details Initializes the txfile to an empty file.
   * \param[in] name The name under which instrumentation is to be published.
   * \param[in] fd The file descriptor of the file.
   * \param[in] off The offset at which the DB is found.
   * \param[in] wal_len The length in bytes of the WAL.
   */
  static auto create(std::string name, monsoon::io::fd&& fd, monsoon::io::fd::offset_type off = 0, monsoon::io::fd::size_type len = DEFAULT_WAL_BYTES, const db_options& options = db_options()) -> std::shared_ptr<db>;

  ///\brief Constructor used during create call.
  monsoon_tx_local_ db(std::string name, txfile&& f, const db_options& options);

  private:
  ///\brief Validate the header in front of the WAL and uses it to load the WAL.
  monsoon_tx_local_ auto validate_header_and_load_wal_(const std::string& name, monsoon::io::fd&& fd, monsoon::io::fd::offset_type off) -> txfile;

  public:
  auto begin(bool read_only) -> transaction;
  auto begin() const -> transaction;

  private:
  txfile f_; // Underlying file.
  std::shared_ptr<detail::commit_manager> cm_; // Allocate transaction IDs.
  cycle_ptr::cycle_gptr<detail::db_cache> obj_cache_;
};


/**
 * \brief Transaction participant.
 * \details
 * Interface for specific types that participate in a transaction.
 */
class monsoon_tx_export_ db::transaction_obj
: public cycle_ptr::cycle_base
{
  friend transaction;

  public:
  virtual ~transaction_obj() noexcept = 0;

  private:
  monsoon_tx_local_ void commit_phase1(detail::commit_manager::write_id& tx);
  monsoon_tx_local_ void commit_phase2(const detail::commit_manager::commit_id& write_id) noexcept;
  monsoon_tx_local_ auto validate(const detail::commit_manager::commit_id& write_id) -> std::error_code;
  monsoon_tx_local_ void rollback() noexcept;

  virtual void do_commit_phase1(detail::commit_manager::write_id& tx);
  virtual void do_commit_phase2(const detail::commit_manager::commit_id& write_id) noexcept;
  virtual auto do_validate(const detail::commit_manager::commit_id& write_id) -> std::error_code;
  virtual void do_rollback() noexcept;
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
  ///\brief Test if we can see the given \p datum.
  auto visible(const cycle_ptr::cycle_gptr<tx_aware_data>& datum) const noexcept -> bool;

  void commit();
  void rollback() noexcept;

  auto active() const noexcept -> bool { return active_; }
  auto read_only() const noexcept -> bool { return read_only_; }
  auto read_write() const noexcept -> bool { return !read_only(); }

  private:
  ///\brief Lock all transaction_obj layouts.
  ///\details Allows us to rely on tx_aware_data offsets to be stable.
  auto lock_all_layouts_() const -> std::unordered_map<cycle_ptr::cycle_gptr<const detail::layout_obj>, std::shared_lock<std::shared_mutex>>;
  ///\brief Execute phase 1 commit on all transaction_obj.
  ///\details This is the phase where writes to disk are prepared.
  void commit_phase1_(detail::commit_manager::write_id& tx);
  ///\brief Execute phase 2 commit.
  ///\details This is the phase where in-memory data is changed to reflect the commit.
  void commit_phase2_(const detail::commit_manager::commit_id& write_id) noexcept;
  ///\brief Test if all objects involved in the transaction.
  ///\details
  ///This is where we test that none of the objects we mutate were mutated by another transaction.
  ///We also test that all objects that must exist at commit time, still exist.
  ///We can also do more complicated validation, such as unique-ness constraints for new objects.
  auto validate_(const detail::commit_manager::commit_id& write_id) -> std::error_code;
  ///\brief Rollback all transaction objects.
  void rollback_() noexcept;

  detail::commit_manager::commit_id seq_;
  bool read_only_;
  bool active_ = false;
  std::unordered_map<cycle_ptr::cycle_gptr<db_obj>, cycle_ptr::cycle_gptr<transaction_obj>> callbacks_;
  std::weak_ptr<db> self_;

  ///\brief Set of objects that are being deleted.
  std::unordered_set<cycle_ptr::cycle_gptr<tx_aware_data>> deleted_set_;
  ///\brief Set of objects that are being created.
  std::unordered_set<cycle_ptr::cycle_gptr<tx_aware_data>> created_set_;
  ///\brief Set of objects that must not be deleted/modified.
  std::unordered_set<cycle_ptr::cycle_gptr<tx_aware_data>> require_set_;
};


class monsoon_tx_export_ db::db_obj {
  friend db::transaction;

  protected:
  db_obj() = delete;
  db_obj(const db_obj&) = delete;
  db_obj& operator=(const db_obj&) = delete;
  db_obj(db_obj&&) = delete;
  db_obj& operator=(db_obj&&) = delete;

  db_obj(std::shared_ptr<class db> db);
  virtual ~db_obj() noexcept = 0;

  ///\brief The object cache of the database.
  const cycle_ptr::cycle_gptr<detail::db_cache> obj_cache;
  ///\brief Acquire the database pointer.
  auto db() const -> std::shared_ptr<class db>;
  ///\brief Begin a txfile transaction.
  auto txfile_begin() const -> txfile::transaction;

  private:
  std::weak_ptr<class db> db_;
};


} /* namespace monsoon::tx */

#include "db-inl.h"

#endif /* MONSOON_TX_DB_H */
