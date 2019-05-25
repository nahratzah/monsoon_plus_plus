#ifndef MONSOON_HISTORY_DIR_IO_TXFILE_H
#define MONSOON_HISTORY_DIR_IO_TXFILE_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <utility>
#include <vector>
#include <monsoon/io/fd.h>
#include <monsoon/history/dir/io/tx_sequencer.h>
#include <monsoon/history/dir/io/wal.h>
#include <boost/intrusive/set.hpp>
#include <boost/intrusive/options.hpp>

namespace monsoon::history::io {


class txfile_transaction_error
: public std::runtime_error
{
  public:
  using std::runtime_error::runtime_error;
  ~txfile_transaction_error();
};

class txfile_bad_transaction
: public std::logic_error
{
  public:
  using std::logic_error::logic_error;
  ~txfile_bad_transaction();
};

class txfile_read_only_transaction
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
class txfile {
  private:
  class replacement_map {
    private:
    using vector_type = std::vector<std::uint8_t>;

    struct entry_type
    : public boost::intrusive::set_base_hook<>
    {
      entry_type(monsoon::io::fd::offset_type first, vector_type second) noexcept
      : first(std::move(first)),
        second(std::move(second))
      {}

      auto end_offset() const noexcept -> monsoon::io::fd::offset_type {
        return first + second.size();
      }

      const monsoon::io::fd::offset_type first;
      const vector_type second;
    };

    struct entry_key_extractor_ {
      using type = monsoon::io::fd::offset_type;

      auto operator()(const entry_type& e) const noexcept -> const type& {
        return e.first;
      }
    };

    using map_type = boost::intrusive::set<
        entry_type,
        boost::intrusive::key_of_value<entry_key_extractor_>,
        boost::intrusive::constant_time_size<false>>;

    public:
    ///\brief Opaque type used by txfile to allow transaction semantics for the replacement_map.
    class tx;

    ~replacement_map() noexcept;

    /**
     * \brief Read data from the replacement map if applicable.
     * \details
     * Data in the replacement map is sparse.
     * Thus not all reads may succeed.
     * When a read doesn't succeed, we'll indicate that by returning a zero-bytes-read return value.
     *
     * \param[in] off The offset at which the read happens.
     * \param[out] buf Buffer into which to read data.
     * \param[in,out] nbytes Number of bytes to read. If the read happens inside a gap of the replacement map, the value will be clamped to not exceed that gap.
     * \return The number of bytes read from the replacement map, or 0 if no bytes could be read.
     */
    auto read_at(monsoon::io::fd::offset_type off, void* buf, std::size_t& nbytes) const -> std::size_t;
    /**
     * \brief Write data into the replacement map.
     * \details
     * Prepares a transaction that writes \p nbytes from \p buf into the replacement map.
     *
     * If the map already holds data at the given position, it will be replaced.
     *
     * The returned transaction is applied only if the commit method is invoked.
     * The replacement_map only allows for a single transaction at a time.
     * \throw std::overflow_error if \p buf + \p nbytes exceed the range of an offset.
     * \throw std::bad_alloc if insufficient memory is available to complete the operation.
     */
    auto write_at(monsoon::io::fd::offset_type off, const void* buf, std::size_t nbytes) -> tx;

    private:
    map_type map_;
  };

  public:
  class transaction;

  txfile(const txfile&) = delete;
  txfile& operator=(const txfile&) = delete;

  ///\brief Create a new txfile object that does not point at a file.
  txfile() noexcept = default;

  /**
   * \brief Open an existing txfile.
   * \details
   * \param[in] fd The file descriptor of the file.
   * \param[in] off The offset at which the WAL is found.
   * \param[in] len The length in bytes of the WAL.
   */
  txfile(monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len);

  private:
  struct create_tag {};
  txfile(create_tag tag, monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len);

  public:
  txfile(txfile&&) noexcept = default;
  txfile& operator=(txfile&&) noexcept = default;

  auto create(monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len) -> txfile;

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
   * \brief Start a new read-only transaction.
   * \details
   * Start a new transaction.
   * Because the txfile is const, only a read-only transaction can be started.
   * \return A new read-only transaction.
   */
  auto begin() const -> transaction;

  private:
  struct impl_ {
    impl_(const impl_&) = delete;
    impl_(impl_&&) = delete;
    impl_& operator=(const impl_&) = delete;
    impl_& operator=(impl_&&) = delete;

    impl_(monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len)
    : fd_(std::move(fd)),
      wal_(fd_, off, len)
    {}

    impl_([[maybe_unused]] create_tag tag, monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len)
    : fd_(std::move(fd)),
      wal_(wal_region::create(fd_, off, len))
    {}

    ~impl_() noexcept;

    monsoon::io::fd fd_;
    wal_region wal_;
    tx_sequencer sequencer_;
    ///\brief Mutex guards the file contents, except for the WAL regions.
    std::shared_mutex mtx_;
  };

  std::unique_ptr<impl_> pimpl_;
};


/**
 * \brief Transaction object for transactional files.
 * \details
 * This object is used to interact with the contents of a txfile instance.
 */
class txfile::transaction {
  friend txfile;

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
    seq_(std::move(x.seq_))
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
    return *this;
  }

  private:
  /**
   * \brief Internal transaction constructor.
   * \details
   * This constructor is used by txfile to begin a new transaction.
   */
  explicit transaction(bool read_only, txfile& owner)
  : read_only_(read_only),
    owner_(owner.pimpl_.get()),
    seq_(owner.pimpl_->sequencer_.begin())
  {}

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

  auto write_at(offset_type off, const void* buf, std::size_t nbytes) -> std::size_t;
  auto read_at(offset_type off, void* buf, std::size_t nbytes) const -> std::size_t;

  private:
  bool read_only_;
  impl_* owner_ = nullptr;
  tx_sequencer::tx seq_;
  ///\brief Hold the result of uncommitted write actions.
  replacement_map replacements_;
  ///\brief Hold on to the WAL transaction ID.
  std::optional<wal_record::tx_id_type> tx_id_;
};


inline auto txfile::begin(bool read_only) -> transaction {
  return transaction(read_only, *this);
}

inline auto txfile::begin() const -> transaction {
  return const_cast<txfile&>(*this).begin(true);
}


} /* namespace monsoon::history::io */

#endif /* MONSOON_HISTORY_DIR_IO_TXFILE_H */
