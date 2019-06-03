#ifndef MONSOON_HISTORY_DIR_IO_WAL_H
#define MONSOON_HISTORY_DIR_IO_WAL_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <stdexcept>
#include <vector>
#include <monsoon/io/fd.h>
#include <monsoon/io/stream.h>
#include <monsoon/xdr/xdr.h>
#include <monsoon/history/dir/io/replacement_map.h>

namespace monsoon::history::io {


/**
 * \brief Exception for Write-Ahead-Log.
 * \details
 * This error indicates that the WAL encountered an unrecoverable error.
 * When encountered, the WAL becomes unusable.
 */
class monsoon_dirhistory_export_ wal_error
: public std::runtime_error
{
  public:
  using std::runtime_error::runtime_error;
  ~wal_error();
};

/**
 * \brief Exception for Write-Ahead-Log being filled.
 * \details
 * This error indicates a write to the WAL failed, due to the WAL having
 * no more space to write log entries.
 */
class monsoon_dirhistory_export_ wal_bad_alloc
: public wal_error
{
  public:
  using wal_error::wal_error;
  ~wal_bad_alloc();
};


class wal_region;


///\brief Types of WAL entries.
enum class wal_entry : std::uint8_t {
  end = 0, ///<\brief End of WAL segment.
  commit = 1, ///<\brief Transaction commit.
  write = 10, ///<\brief Write operation that is part of a transaction.
  resize = 11 ///<\brief File resize operation that is part of a transaction.
};


/**
 * \brief Entry in the WAL.
 * \details
 * WAL records describe a single operation.
 */
class monsoon_dirhistory_export_ wal_record {
  public:
  ///\brief Type of transaction IDs.
  using tx_id_type = std::uint32_t;

  ///\brief Mask for transaction IDs.
  ///\details Transactions are encoded as part of the WAL record type, using the high 24 bit.
  static constexpr tx_id_type tx_id_mask = 0xffffffu;

  wal_record() = delete;
  ///\brief Construct a WAL record with the given transaction ID.
  wal_record(tx_id_type tx_id);
  ///\brief Destructor.
  virtual ~wal_record() noexcept;
  ///\brief Return the WAL entry type.
  virtual auto get_wal_entry() const noexcept -> wal_entry = 0;
  private:
  ///\brief Write this record to an XDR stream.
  virtual auto do_write(monsoon::xdr::xdr_ostream& out) const -> void = 0;
  ///\brief Apply the operation described in this WAL record.
  virtual auto do_apply(wal_region& wal) const -> void = 0;
  public:
  ///\brief Read a WAL record from an XDR stream.
  static auto read(monsoon::xdr::xdr_istream& in) -> std::unique_ptr<wal_record>;
  ///\brief Write this recod to an XDR stream.
  void write(monsoon::xdr::xdr_ostream& out) const;
  ///\brief Raw operation that writes a header for an entry.
  static void to_stream(monsoon::xdr::xdr_ostream& out, wal_entry e, tx_id_type tx_id);
  ///\brief Apply the operation described in this WAL record.
  void apply(wal_region& wal) const;
  ///\brief Test if this WAL record denotes the end of a WAL segment.
  auto is_end() const noexcept -> bool;
  ///\brief Test if this WAL record indicates a transaction commit.
  auto is_commit() const noexcept -> bool;
  ///\brief Test if this WAL record is a control record.
  auto is_control_record() const noexcept -> bool;
  ///\brief Retrieve the transaction ID of this WAL record.
  auto tx_id() const noexcept -> std::uint32_t { return tx_id_; }

  ///\brief Create a record describing the end of a WAL segment.
  static auto make_end() -> std::unique_ptr<wal_record>;
  ///\brief Create a record describing a transaction commit.
  ///\params[in] tx_id The transaction ID.
  static auto make_commit(tx_id_type tx_id) -> std::unique_ptr<wal_record>;
  ///\brief Create a record that describes a write operation.
  ///\params[in] tx_id The transaction ID.
  ///\params[in] offset The position at which the write happens.
  ///\params[in] data The data that is to be written.
  static auto make_write(tx_id_type tx_id, std::uint64_t offset, std::vector<std::uint8_t>&& data) -> std::unique_ptr<wal_record>;
  ///\brief Create a record that describes a write operation.
  ///\params[in] tx_id The transaction ID.
  ///\params[in] offset The position at which the write happens.
  ///\params[in] data The data that is to be written.
  static auto make_write(tx_id_type tx_id, std::uint64_t offset, const std::vector<std::uint8_t>& data) -> std::unique_ptr<wal_record>;
  ///\brief Create a record indicating the file is being resized.
  ///\params[in] tx_id The transaction ID.
  ///\params[in] new_size The new size of the file.
  static auto make_resize(tx_id_type tx_id, std::uint64_t new_size) -> std::unique_ptr<wal_record>;

  private:
  ///\brief The transaction ID of the record.
  tx_id_type tx_id_;
};


class wal_record_end;
class wal_record_commit;
class wal_record_write;
class wal_record_resize;


/**
 * \brief A WAL region in a file.
 * \details
 * The WAL region handles the logistics of making a file appear transactional.
 */
class monsoon_dirhistory_export_ wal_region {
  friend wal_record_write;
  friend wal_record_resize;

  private:
  struct wal_header;

  public:
  ///\brief Constructor tag signaling a newly created file.
  struct create {};
  class tx;

  private:
  ///\brief WAL segment sequence number type.
  using wal_seqno_type = std::uint32_t;

  ///\brief A WAL segment.
  ///\details This type holds information during transaction replay.
  struct wal_vector {
    ///\brief Slot index.
    std::size_t slot;
    ///\brief WAL segment sequence number.
    wal_seqno_type seq;
    ///\brief Size of the file at the start of the segment.
    monsoon::io::fd::size_type file_size;
    ///\brief Records in the WAL segment.
    std::vector<std::unique_ptr<wal_record>> data;
  };

  public:
  wal_region() = delete;
  ///\brief Create a WAL region from an existing file.
  ///\param[in] fd The file in which to open the region.
  ///\param[in] off The offset in the file at which the WAL was created.
  ///\param[in] len The size of the WAL.
  wal_region(monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len);
  ///\brief Create a WAL region from a newly initialized file.
  ///\param[in] c A tag type to distinguish between the constructors.
  ///\param[in] fd The file in which to open the region.
  ///\param[in] off The offset in the file at which to create the WAL.
  ///\param[in] len The size of the WAL.
  wal_region(create c, monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len);
  wal_region(wal_region&&) noexcept = delete;
  wal_region(const wal_region&) = delete;
  wal_region& operator=(wal_region&&) noexcept = delete;
  wal_region& operator=(const wal_region&) = delete;
  ~wal_region() = default;

  ///\brief Retrieve the end of the WAL region.
  auto wal_end_offset() const noexcept -> monsoon::io::fd::offset_type {
    return off_ + len_;
  }

  private:
  ///\brief Allocate a transaction ID.
  [[nodiscard]]
  auto allocate_tx_id() -> wal_record::tx_id_type;

  ///\brief Number of segments that the WAL is divided in.
  static constexpr std::size_t num_segments_ = 2;

  ///\brief Length of the segments inside the WAL.
  auto segment_len_() const noexcept -> monsoon::io::fd::size_type {
    return segment_len_(len_);
  }

  ///\brief Length of the segments inside the WAL.
  static constexpr auto segment_len_(monsoon::io::fd::size_type len) noexcept -> monsoon::io::fd::size_type {
    return len / num_segments_;
  }

  ///\brief Retrieve the begin offset of a given segment.
  ///\param[in] slot The index of the slot for which to deduce the offset.
  ///\return The offset of the first byte of this slot.
  auto slot_begin_off(std::size_t slot) const noexcept -> monsoon::io::fd::offset_type {
    return off_ + slot * segment_len_();
  }

  ///\brief Retrieve the end offset of a given segment.
  ///\param[in] slot The index of the slot for which to deduce the offset.
  ///\return The offset of the first past-the-end byte of this slot.
  auto slot_end_off(std::size_t slot) const noexcept -> monsoon::io::fd::offset_type {
    return slot_begin_off(slot) + segment_len_();
  }

  ///\brief Read a WAL segment header.
  ///\param[in] fd The file descriptor.
  ///\param[in] idx The slot index of the segment to read.
  auto read_segment_header_(std::size_t idx) const -> wal_header;
  ///\brief Read a WAL segment.
  ///\param[in] fd The file descriptor.
  ///\param[in] idx The slot index of the segment to read.
  auto read_segment_(std::size_t idx) const -> wal_vector;

  public:
  auto fd() const & noexcept -> const monsoon::io::fd& {
    return fd_;
  }

  auto fd() & noexcept -> monsoon::io::fd& {
    return fd_;
  }

  auto fd() && -> monsoon::io::fd&& {
    return std::move(fd_);
  }

  auto read_at(monsoon::io::fd::offset_type off, void* buf, std::size_t len) const -> std::size_t;
  void compact();
  auto size() const noexcept -> monsoon::io::fd::size_type;

  private:
  ///\brief Read from the WAL log.
  ///\details Must be called with mtx_ held for share.
  ///\param[in] off File offset at which the read happens.
  ///\param[in] buf Buffer into which to read.
  ///\param[in] len The length of the read.
  ///\return The number of bytes read.
  auto read_at_(monsoon::io::fd::offset_type off, void* buf, std::size_t len) const -> std::size_t;
  ///\brief Write a WAL record to the log.
  ///\param[in] r The record to write.
  ///This should only be set when copying into a new log, until the log is activated.
  void log_write_(const wal_record& r);
  ///\brief Write a WAL record to the log, that is encoded in the given byte sequence.
  ///\param[in] xdr The raw bytes of zero or more WAL records, with a wal_record_end following it.
  ///This should only be set when copying into a new log, until the log is activated.
  void log_write_raw_(const monsoon::xdr::xdr_bytevector_ostream<>& xdr);
  /**
   * \brief Compact the log.
   * \details
   * Reads the log, filters out all transactions that have completed,
   * and writes it out again. This action compresses the log, making
   * space available for writes.
   *
   * During compaction, all pending writes will be flushed out as well.
   */
  void compact_();
  ///\brief Write a WAL record for a write to the log.
  ///\details This is equivalent to calling
  ///`log_write(wal_record_write(...))`.
  ///This method elides a copy operation of the buffer, making it a bit faster
  ///to execute.
  ///\param[in] tx_id The transaction ID of the write operation.
  ///\param[in] off The offset at which the write takes place.
  ///\param[in] buf The buffer holding the data that is to be written.
  ///\param[in] len The length of the buffer.
  void tx_write_(wal_record::tx_id_type tx_id, monsoon::io::fd::offset_type off, const void* buf, std::size_t len);
  ///\brief Write a WAL record for a resize operation to the log.
  ///\param[in] tx_id The transaction ID of the write operation.
  ///\param[in] new_size The new file size.
  void tx_resize_(wal_record::tx_id_type tx_id, monsoon::io::fd::size_type new_size);
  ///\brief Write a commit message to the log.
  ///\param[in] tx_id The transaction ID of the commit operation.
  ///\param[in] writes The writes done as part of this transaction.
  ///\param[in] new_file_size If present, a file size modification operation.
  ///\return A replacement_map recording for all replaced data in the file, what the before-commit contents was.
  void tx_commit_(wal_record::tx_id_type tx_id, replacement_map&& writes, std::optional<monsoon::io::fd::size_type> new_file_size, std::function<void(replacement_map)> undo_op_fn);
  ///\brief Mark a transaction as canceled.
  void tx_rollback_(wal_record::tx_id_type tx_id) noexcept;

  ///\brief Offset of the WAL.
  const monsoon::io::fd::offset_type off_;
  ///\brief Length of the WAL.
  const monsoon::io::fd::size_type len_;
  ///\brief WAL segment sequence number.
  wal_seqno_type current_seq_;
  ///\brief Current WAL segment slot to which records are appended.
  std::size_t current_slot_;
  ///\brief Append offset in the slot.
  monsoon::io::fd::offset_type slot_off_;

  ///\brief Vector where tx_id is the index and bool indicates wether the transaction is in progress.
  ///\details A transaction that is in progress has been started, but has neither been committed, nor been rolled back.
  std::vector<bool> tx_id_states_;
  ///\brief List of transaction IDs that are available for allocation.
  ///\details
  ///These IDs are all marked as inactive.
  std::priority_queue<wal_record::tx_id_type, std::vector<wal_record::tx_id_type>, std::greater<wal_record::tx_id_type>> tx_id_avail_;
  ///\brief Number of completed transactions in tx_id_states_.
  ///\details
  ///This holds the value `std::count(tx_id_states_.cbegin(), tx_id_states_.cend(), false)`.
  std::vector<bool>::size_type tx_id_completed_count_ = 0;

  ///\brief Mutex providing read/write access to the file, excluding the WAL.
  ///\details
  ///Parts of the file covered by repl_ are not protected with this mutex (but repl_ itself is).
  ///Instead, the log_mtx_ covers those sections.
  ///
  ///This mutex may not be locked with alloc_mtx_ held.
  mutable std::shared_mutex mtx_;
  ///\brief Mutex providing access to the WAL.
  ///\details
  ///This mutex may not be locked with mtx_ or alloc_mtx_ held.
  mutable std::mutex log_mtx_;
  ///\brief Mutex providing access to the allocator data.
  ///\details
  ///Protects tx_id_states_ and tx_id_avail_.
  mutable std::mutex alloc_mtx_;
  ///\brief File descriptor.
  monsoon::io::fd fd_;
  ///\brief Current size of the file.
  monsoon::io::fd::size_type fd_size_;
  ///\brief Pending writes.
  replacement_map repl_;
};


/**
 * \brief Transaction for WAL region.
 * \details
 * A WAL transaction runs at read-committed isolation.
 */
class monsoon_dirhistory_export_ wal_region::tx {
  public:
  tx() = default;
  tx(const tx&) = delete;
  tx(tx&&) noexcept = default;
  tx& operator=(const tx&) = delete;
  tx& operator=(tx&&) noexcept = default;

  ///\brief Start a new transaction.
  tx(const std::shared_ptr<wal_region>& wal) noexcept;

  ///\brief Destructor.
  ~tx() noexcept;

  ///\brief Test if this transaction is active.
  explicit operator bool() const noexcept;

  ///\brief Test if this transaction is invalid.
  auto operator!() const noexcept -> bool {
    return !this->operator bool();
  }

  ///\brief Transactional write.
  ///\param[in] off The offset at which to write.
  ///\param[in] buf Buffer with data.
  ///\param[in] len The size of the buffer.
  ///\throws std::bad_weak_ptr if the transaction is invalid.
  void write_at(monsoon::io::fd::offset_type off, const void* buf, std::size_t len);

  ///\brief Transactional resize operation.
  ///\details Allows for the file to grow or shrink.
  ///\param[in] new_size The new file size.
  ///\throws std::bad_weak_ptr if the transaction is invalid.
  void resize(monsoon::io::fd::size_type new_size);

  ///\brief Commit this transaction.
  ///\param[in] undo_op_fn A callback that will accept the replacement_map with recorded overwritten data.
  ///\throws std::bad_weak_ptr if the transaction is invalid.
  void commit(std::function<void(replacement_map)> undo_op_fn);
  ///\brief Commit this transaction.
  ///\throws std::bad_weak_ptr if the transaction is invalid.
  void commit();

  ///\brief Rollback this transaction.
  void rollback() noexcept;

  ///\brief Read operation.
  ///\details Performs a read.
  ///The data visible to the read operation is the set of committed transactions.
  ///
  ///The \p fn callback is invoked between reading the transaction-local information
  ///and the WAL-committed information.
  ///\param[in] off The file offset to read from.
  ///\param[out] buf The buffer to read data into.
  ///\param[in] len The size of the buffer.
  ///\param[in] fn An invokable that is to participate in the read_at operation.
  ///\return The number of bytes read.
  ///\throws std::bad_weak_ptr if the transaction is invalid.
  template<typename IntermediateFn>
  auto read_at(monsoon::io::fd::offset_type off, void* buf, std::size_t len, IntermediateFn&& fn) const -> std::size_t {
    const auto wal = std::shared_ptr<wal_region>(wal_);

    // If the transaction has an altered file size, apply it.
    if (new_file_size_.has_value()) {
      if (off >= *new_file_size_) return 0;
      if (len > *new_file_size_ - off) len = *new_file_size_ - off;
    }

    // First, evaluate local writes.
    {
      const auto local_rlen = writes_.read_at(off, buf, len); // May update len.
      if (local_rlen != 0u) return local_rlen;
    }

    // The callback and the WAL both are protected using the mtx_.
    std::shared_lock<std::shared_mutex> lck{ wal->mtx_ };

    // Second, evaluate callback read operation.
    {
      const auto fn_rlen = std::invoke(std::forward<IntermediateFn>(fn), off, buf, len); // May update len.
      if (fn_rlen != 0u) return fn_rlen;
    }

    // Third, read directly from the WAL.
    {
      const auto wal_rlen = wal->read_at_(off, buf, len);
      if (wal_rlen != 0u) return wal_rlen;
    }

    // If nothing can provide data, pretend the file is zero-filled.
    if (new_file_size_.has_value()) {
      std::fill_n(reinterpret_cast<std::uint8_t*>(buf), len, std::uint8_t(0u));
      return len;
    }

    return 0;
  }

  ///\brief Read operation.
  ///\details Performs a read.
  ///The data visible to the read operation is the set of committed transactions.
  ///\param[in] off The file offset to read from.
  ///\param[out] buf The buffer to read data into.
  ///\param[in] len The size of the buffer.
  ///\return The number of bytes read.
  ///\throws std::bad_weak_ptr if the transaction is invalid.
  auto read_at(monsoon::io::fd::offset_type off, void* buf, std::size_t len) const -> std::size_t;

  ///\brief Get the size of the file.
  ///\return The size of the file.
  ///\throws std::bad_weak_ptr if the transaction is invalid.
  auto size() const -> monsoon::io::fd::size_type;

  private:
  ///\brief Reference to the WAL.
  std::weak_ptr<wal_region> wal_;
  ///\brief Writes performed in this transaction.
  replacement_map writes_;
  ///\brief Recorded change in file size.
  std::optional<monsoon::io::fd::size_type> new_file_size_;
  ///\brief Internal transaction ID.
  wal_record::tx_id_type tx_id_;
};


} /* namespace monsoon::history::io */

#endif /* MONSOON_HISTORY_DIR_IO_WAL_H */
