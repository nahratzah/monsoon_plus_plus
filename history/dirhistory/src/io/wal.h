#ifndef IO_WAL_H
#define IO_WAL_H

#include <algorithm>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>
#include <monsoon/io/fd.h>
#include <monsoon/io/stream.h>
#include <monsoon/xdr/xdr.h>

namespace monsoon::history::io {


/**
 * \brief Exception for Write-Ahead-Log.
 * \details
 * This error indicates that the WAL encountered an unrecoverable error.
 * When encountered, the WAL becomes unusable.
 */
class wal_error
: public std::runtime_error
{
  public:
  using std::runtime_error::runtime_error;
  ~wal_error();
};


///\brief Types of WAL entries.
enum class wal_entry : std::uint8_t {
  end = 0, ///<\brief End of WAL segment.
  commit = 1, ///<\brief Transaction commit.
  invalidate_previous_wal = 2, ///<\brief Preceding WAL segments have been processed and consequently are invalidated.
  write = 10, ///<\brief Write operation that is part of a transaction.
  resize = 11, ///<\brief File resize operation that is part of a transaction.
  copy = 20 ///<\brief Copy operation, where contents from within the file is copied, as part of a transaction.
};


/**
 * \brief Entry in the WAL.
 * \details
 * WAL records describe a single operation.
 */
class wal_record {
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
  virtual auto do_apply(monsoon::io::fd& fd) const -> void = 0;
  public:
  ///\brief Read a WAL record from an XDR stream.
  static auto read(monsoon::xdr::xdr_istream& in) -> std::unique_ptr<wal_record>;
  ///\brief Write this recod to an XDR stream.
  void write(monsoon::xdr::xdr_ostream& out) const;
  ///\brief Apply the operation described in this WAL record.
  void apply(monsoon::io::fd& fd) const;
  ///\brief Test if this WAL record denotes the end of a WAL segment.
  auto is_end() const noexcept -> bool;
  ///\brief Test if this WAL record indicates a transaction commit.
  auto is_commit() const noexcept -> bool;
  ///\brief Test if this WAL record indicates preceding WAL segments have been processed and are invalidated.
  auto is_invalidate_previous_wal() const noexcept -> bool;
  ///\brief Retrieve the transaction ID of this WAL record.
  auto tx_id() const noexcept -> std::uint32_t { return tx_id_; }

  ///\brief Create a record describing the end of a WAL segment.
  static auto make_end() -> std::unique_ptr<wal_record>;
  ///\brief Create a record describing a transaction commit.
  ///\params[in] tx_id The transaction ID.
  static auto make_commit(tx_id_type tx_id) -> std::unique_ptr<wal_record>;
  ///\brief Create a record that invalidates preceding WAL segments.
  static auto make_invalidate_previous_wal() -> std::unique_ptr<wal_record>;
  ///\brief Create a record that describes a write operation.
  ///\params[in] tx_id The transaction ID.
  ///\params[in] offset The position at which the write happens.
  ///\params[in] data The data that is to be written.
  static auto make_write(tx_id_type tx_id, std::uint64_t offset, std::vector<uint8_t>&& data) -> std::unique_ptr<wal_record>;
  ///\brief Create a record that describes a write operation.
  ///\params[in] tx_id The transaction ID.
  ///\params[in] offset The position at which the write happens.
  ///\params[in] data The data that is to be written.
  static auto make_write(tx_id_type tx_id, std::uint64_t offset, const std::vector<uint8_t>& data) -> std::unique_ptr<wal_record>;
  ///\brief Create a record indicating the file is being resized.
  ///\params[in] tx_id The transaction ID.
  ///\params[in] new_size The new size of the file.
  static auto make_resize(tx_id_type tx_id, std::uint64_t new_size) -> std::unique_ptr<wal_record>;
  ///\brief Create a record that indicates a copy operation.
  ///\params[in] tx_id The transaction ID.
  ///\params[in] src The offset at which the data that is to be copied is stored.
  ///\params[in] dst The offset at which the data is to be written.
  ///\params[in] len The amount of bytes that are to be written.
  static auto make_copy(tx_id_type tx_id, std::uint64_t src, std::uint64_t dst, std::uint64_t len) -> std::unique_ptr<wal_record>;

  private:
  ///\brief The transaction ID of the record.
  tx_id_type tx_id_;
};


///\brief Helper type that ensures a read operation spans a specific byte range.
class wal_reader
: public monsoon::io::stream_reader
{
  public:
  ///\brief Default constructor create an invalid wal_reader.
  wal_reader() noexcept = default;

  /**
   * \brief Create a new WAL reader.
   * \details
   * Only \p len bytes at \p off in the file \p fd are available for reading.
   * \param[in] fd File descriptor to read from. Caller must ensure the file descriptor remains valid.
   * \param[in] off Offset in the file at which reading should start.
   * \param[in] len The number of bytes to read.
   * \throws wal_error If an attempt is made to read at the end of the segment.
   */
  wal_reader(const monsoon::io::fd& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len)
  : fd_(&fd),
    off_(off),
    len_(len)
  {}

  wal_reader(const wal_reader& other) noexcept = default;

  wal_reader(wal_reader&& other) noexcept
  : fd_(std::exchange(other.fd_, nullptr)),
    off_(std::exchange(other.off_, 0u)),
    len_(std::exchange(other.len_, 0u))
  {}

  wal_reader& operator=(const wal_reader&) noexcept = default;

  wal_reader& operator=(wal_reader&& other) noexcept {
    fd_ = std::exchange(other.fd_, nullptr);
    off_ = std::exchange(other.off_, 0);
    len_ = std::exchange(other.len_, 0);
    return *this;
  }

  ~wal_reader() noexcept override;

  auto read(void* buf, std::size_t nbytes) -> std::size_t override;
  void close() override;
  auto at_end() const -> bool override;

  private:
  const monsoon::io::fd* fd_ = nullptr; // No ownership.
  monsoon::io::fd::offset_type off_ = 0;
  monsoon::io::fd::size_type len_ = 0;
};


/**
 * \brief A WAL region in a file.
 * \details
 * The WAL region handles the logistics of making a file appear transactional.
 */
class wal_region {
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
    ///\brief Records in the WAL segment.
    std::vector<std::unique_ptr<wal_record>> data;
  };

  public:
  ///\brief Default constructor creates an invalid WAL region.
  wal_region() = default;
  ///\brief Create a WAL region.
  ///\param[in] fd The file in which to open the region.
  ///\param[in] off The offset in the file at which the WAL was created.
  ///\param[in] len The size of the WAL.
  wal_region(monsoon::io::fd& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len);
  ///\brief A WAL is move-constructible.
  wal_region(wal_region&&) = default;
  wal_region(const wal_region&) = delete;
  ///\brief A WAL region is move-assignable.
  wal_region& operator=(wal_region&&) = default;
  wal_region& operator=(const wal_region&) = delete;
  ~wal_region() = default;

  ///\brief Initialize a WAL region.
  ///\param[in] fd The file in which to create the region.
  ///\param[in] off The offset in the file at which to create the WAL.
  ///\param[in] len The size of the WAL.
  static auto create(monsoon::io::fd& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len) -> wal_region;

  private:
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

  ///\brief Read a WAL segment.
  ///\param[in] idx The slot index of the segment to read.
  auto read_segment_(std::size_t idx) -> wal_vector;
  ///\brief Create an in-memory representation of an empty segment.
  static auto make_empty_segment_(wal_seqno_type seq, bool invalidate) -> monsoon::xdr::xdr_bytevector_ostream<>;

  ///\brief File descriptor in which the WAL resides.
  monsoon::io::fd* fd_ = nullptr;
  ///\brief Offset of the WAL.
  monsoon::io::fd::offset_type off_;
  ///\brief Length of the WAL.
  monsoon::io::fd::size_type len_;
  ///\brief WAL segment sequence number.
  wal_seqno_type current_seq_;
  ///\brief Current WAL segment slot to which records are appended.
  std::size_t current_slot_;
  ///\brief Bitset indicating for each slot if it is active or inactive.
  ///\details
  ///Active slots hold data that is required during replay.
  ///Inactive slots host invalidated data only and should not be replayed.
  std::bitset<num_segments_> slots_active_{ 0ull };
  ///\brief Append offset in the slot.
  monsoon::io::fd::offset_type slot_off_;
};


} /* namespace monsoon::history::io */

#endif /* IO_WAL_H */
