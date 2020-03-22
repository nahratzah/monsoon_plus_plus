#ifndef MONSOON_TX_TXFILE_H
#define MONSOON_TX_TXFILE_H

#include <monsoon/tx/detail/export_.h>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <monsoon/io/fd.h>
#include <monsoon/tx/detail/tx_sequencer.h>
#include <monsoon/tx/detail/wal.h>
#include <monsoon/tx/detail/replacement_map.h>

namespace monsoon::tx {


class sequence;


class monsoon_tx_export_ txfile_transaction_error
: public std::runtime_error
{
  public:
  using std::runtime_error::runtime_error;
  ~txfile_transaction_error();
};

class monsoon_tx_export_ txfile_bad_transaction
: public std::logic_error
{
  public:
  using std::logic_error::logic_error;
  ~txfile_bad_transaction();
};

class monsoon_tx_export_ txfile_read_only_transaction
: public txfile_bad_transaction
{
  public:
  using txfile_bad_transaction::txfile_bad_transaction;
  ~txfile_read_only_transaction();
};


/**
 * \brief A transactional file.
 * \details
 * A transactional file works by creating the illusion of a file that is
 * modified by atomically committable transactions.
 *
 * The model of the transaction isolation implemented by this file is the repeatable read isolation:
 * - the transaction will see all of the data committed before it started.
 * - the transaction will see none of the data committed before it started.
 *
 * Furthermore, this class guarantees that data that was successfully committed
 * will be stored on disk even in the event of a crash of the binary or the system.
 *
 * The txfile achieves this by using a WAL (Write Ahead Log) to record operations.
 * A designated region holds this WAL.
 *
 * Operations on the logical view of the file pretend the WAL doesn't exist,
 * by translating all offset for file operations to the end of the WAL.
 * For instance: a read or write at offset 0 would correspond to the first byte
 * after the WAL in the actual file.
 *
 * The number of transactions and the size of modifications by them are limited
 * by the size of the WAL.
 * If the WAL region fills up and can't be compacted, any transactions in flight
 * may error out until enough space becomes available.
 *
 * The transactional file has a concept of file size.
 * Only one transaction at a time can change the file size.
 */
class monsoon_tx_export_ txfile {
  friend sequence;

  public:
  ///\brief Transaction sequence ID.
  using id_type = std::uint64_t;

  class transaction;

  txfile(const txfile&) = delete;
  txfile& operator=(const txfile&) = delete;

  ///\brief Create a new txfile object that does not point at a file.
  txfile() noexcept = default;

  /**
   * \brief Open an existing txfile.
   * \details Recovers the file.
   * \param[in] name The name under which instrumentation is to be published.
   * \param[in] fd The file descriptor of the file.
   * \param[in] off The offset at which the WAL is found.
   * \param[in] len The length in bytes of the WAL.
   */
  txfile(std::string name, monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len);

  private:
  using create_tag = detail::wal_region::create;
  txfile(std::string name, create_tag tag, monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len);

  public:
  txfile(txfile&&) noexcept = default;
  txfile& operator=(txfile&&) noexcept = default;

  /**
   * \brief Initialize a txfile.
   * \details Initializes the txfile to an empty file.
   * \param[in] name The name under which instrumentation is to be published.
   * \param[in] fd The file descriptor of the file.
   * \param[in] off The offset at which the WAL is found.
   * \param[in] len The length in bytes of the WAL.
   */
  static auto create(std::string name, monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len) -> txfile;

  /**
   * \brief Start a new transaction.
   * \details
   * Start a new transaction.
   * If the \p read_only parameter is omitted, the code defaults to a read-only transaction.
   * \param[in] read_only If set, the transaction shall be a read-only transaction.
   * \return A new transaction.
   */
  auto begin(bool read_only) -> transaction;
  /**
   * \brief Start a new transaction.
   * \details
   * Start a new transaction.
   * If the \p read_only parameter is omitted, the code defaults to a read-only transaction.
   * \param[in] read_only If set, the transaction shall be a read-only transaction.
   * \param[in] s A sequence for allocating a transaction ID.
   * \return A new transaction.
   */
  auto begin(sequence& s, bool read_only) -> std::tuple<transaction, id_type>;
  /**
   * \brief Start a new read-only transaction.
   * \details
   * Start a new transaction.
   * Because the txfile is const, only a read-only transaction can be started.
   * \return A new read-only transaction.
   */
  auto begin() const -> transaction;
  /**
   * \brief Start a new read-only transaction.
   * \details
   * Start a new transaction.
   * Because the txfile is const, only a read-only transaction can be started.
   * \param[in] s A sequence for allocating a transaction ID.
   * \return A new read-only transaction.
   */
  auto begin(sequence& s) const -> std::tuple<transaction, id_type>;

  private:
  struct monsoon_tx_local_ impl_ {
    impl_(const impl_&) = delete;
    impl_(impl_&&) = delete;
    impl_& operator=(const impl_&) = delete;
    impl_& operator=(impl_&&) = delete;

    impl_(std::string name, monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len)
    : wal_(std::move(name), std::move(fd), off, len)
    {}

    impl_(std::string name, create_tag tag, monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len)
    : wal_(std::move(name), tag, std::move(fd), off, len)
    {}

    ~impl_() noexcept;

    detail::wal_region wal_;
    detail::tx_sequencer sequencer_;
  };

  std::shared_ptr<impl_> pimpl_;
};


/**
 * \brief Transaction object for transactional files.
 * \details
 * This object is used to interact with the contents of a txfile instance.
 */
class monsoon_tx_export_ txfile::transaction {
  friend txfile;
  friend sequence;

  public:
  ///\brief The offset type for the file modeled by the transaction.
  using offset_type = monsoon::io::fd::offset_type;
  ///\brief The size type for the file modeled by the transaction.
  using size_type = monsoon::io::fd::size_type;

  transaction(const transaction&) = delete;
  transaction& operator=(const transaction&) = delete;

  ///\brief Transactions are swappable.
  friend void swap(transaction& x, transaction& y) noexcept {
    using std::swap;
    swap(x.read_only_, y.read_only_);
    swap(x.owner_, y.owner_);
    swap(x.seq_, y.seq_);
    swap(x.wal_, y.wal_);
  }

  /**
   * \brief Create an uninitialized, invalid transaction.
   * \details
   * The transaction is not started and can not have operations applied.
   */
  transaction() = default;

  /**
   * \brief Move construct a transaction.
   */
  transaction(transaction&& x) noexcept
  : read_only_(x.read_only_),
    owner_(std::exchange(x.owner_, nullptr)),
    seq_(std::move(x.seq_)),
    wal_(std::move(x.wal_))
  {}

  /**
   * \brief Move assign a transaction.
   * \details
   * If the current object has an in-progress transaction,
   * that transaction will be rolled back.
   */
  transaction& operator=(transaction&& x) noexcept {
    if (*this) rollback();
    read_only_ = x.read_only_;
    owner_ = std::exchange(x.owner_, nullptr);
    seq_ = std::move(x.seq_);
    wal_ = std::move(x.wal_);
    return *this;
  }

  private:
  /**
   * \brief Internal transaction constructor.
   * \details
   * This constructor is used by txfile to begin a new transaction.
   */
  template<typename CB>
  transaction(bool read_only, const std::shared_ptr<impl_>& owner, CB&& cb);

  public:
  /**
   * \brief Cancel the transaction if it is in progress.
   */
  ~transaction() noexcept {
    if (*this) rollback();
  }

  /**
   * \brief Test if this transaction is valid.
   * \details
   * A valid transaction can execute operations.
   */
  explicit operator bool() const noexcept {
    return owner_ != nullptr;
  }

  /**
   * \brief Test if the transaction is invalid.
   * \details
   * An invalid transaction is one that has been committed or rolled back.
   */
  auto operator!() const noexcept -> bool {
    return !this->operator bool();
  }

  /**
   * \brief Commit this transaction.
   * \details
   * Changes made by this transaction will be visible to transactions started
   * the commit completes.
   *
   * Transactions started before the commit completes won't observe changes
   * made by this transaction.
   */
  void commit();
  /**
   * \brief Cancel this transaction.
   * \details
   * None of the changes made by this transaction will be visible to other
   * transactions.
   */
  void rollback() noexcept;

  void resize(size_type new_size);
  auto size() const -> size_type;
  auto write_at(offset_type off, const void* buf, std::size_t nbytes) -> std::size_t;
  void write_at_many(std::vector<offset_type> off, const void* buf, std::size_t nbytes);
  auto read_at(offset_type off, void* buf, std::size_t nbytes) const -> std::size_t;

  private:
  bool read_only_;
  impl_* owner_ = nullptr;
  ///\brief Hold on to the transaction sequencer.
  detail::tx_sequencer::tx seq_;
  ///\brief Hold on to the WAL transaction.
  detail::wal_region::tx wal_;
};


} /* namespace monsoon::tx */

#endif /* MONSOON_TX_TXFILE_H */
