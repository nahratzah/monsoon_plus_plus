#ifndef MONSOON_TX_DETAIL_TXFILE_ALLOCATOR_LOG_H
#define MONSOON_TX_DETAIL_TXFILE_ALLOCATOR_LOG_H

#include <monsoon/tx/detail/export_.h>
#include <monsoon/tx/detail/tx_op.h>
#include <monsoon/tx/txfile.h>
#include <monsoon/shared_resource_allocator.h>
#include <monsoon/cheap_fn_ref.h>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>
#include <boost/asio/buffer.hpp>

namespace monsoon::tx::detail {


/**
 * \brief Maintain an allocation log for txfile_allocator.
 * \details
 * The log offers a mechanism for recording that memory is being claimed,
 * without requiring the tree to become immutable.
 * It also allows us to allocate memory by growing the file.
 */
class monsoon_tx_export_ txfile_allocator_log
: public std::enable_shared_from_this<txfile_allocator_log>
{
  public:
  using allocator_type = shared_resource_allocator<std::byte>;

  ///\brief The action for a log record.
  enum class action : std::uint8_t {
    skip = 0, ///<\brief Unused entry.
    free = 1, ///<\brief Mark some memory as free.
    used = 2, ///<\brief Mark some memory as being used.
  };

  class log_entry;

  ///\brief Vector returned by the maintenance logic.
  using maintenance_result = std::vector<
      std::shared_ptr<log_entry>,
      std::allocator_traits<allocator_type>::rebind_alloc<std::shared_ptr<log_entry>>>;

  private:
  struct header {
    static constexpr std::uint64_t MAGIC = 0xf45a'8600'd1bf'8eafULL;
    static constexpr std::size_t SIZE = 16;

    ///\brief Magic value.
    std::uint64_t magic;
    ///\brief Offset of first page, or zero if there are no pages.
    std::uint64_t first_page;

    void native_to_big_endian() noexcept;
    void big_to_native_endian() noexcept;
  };

  /**
   * \brief A single allocator log record.
   * \details
   * A log record describes a single action that was taken, but hasn't yet been
   * applied to the txfile_allocator tree.
   */
  struct record {
    ///\brief Number of bytes in the record.
    static constexpr std::size_t SIZE = 24;

    ///\brief Description of the record action.
    std::underlying_type_t<action> act;
    ///\brief Unused bytes.
    std::uint8_t pad0_[8u - sizeof(std::underlying_type_t<action>)];
    ///\brief Address on which the action applies.
    std::uint64_t addr;
    ///\brief Number of bytes on which the action applies.
    std::uint64_t len;

    auto get_action() const -> action { return action(act); }

    void native_to_big_endian() noexcept;
    void big_to_native_endian() noexcept;
    void encode(boost::asio::mutable_buffer buf) const;
    void decode(boost::asio::const_buffer buf);
  };

  /**
   * \brief A single page in the log record.
   * \details
   * A page holds a number of records.
   */
  struct record_page {
    ///\brief Number of bytes in the header.
    static constexpr std::size_t HEADER_SIZE = 16;
    ///\brief Size of the page.
    static constexpr std::size_t SIZE = 64 * 1024;
    ///\brief Number of records in this page.
    static constexpr std::size_t DATA_NELEMS = (SIZE - HEADER_SIZE) / record::SIZE;
    ///\brief Magic for the page.
    ///\details This merely functions to catch bad loads.
    static constexpr std::uint64_t MAGIC = 0xc6ba'33e6'81af'010dULL;

    ///\brief Magic value.
    std::uint64_t magic;
    ///\brief Point to the next page.
    ///\note The value 0 (zero) denotes there is no next page.
    std::uint64_t next_page;
    ///\brief Records in this page.
    record data[DATA_NELEMS];

    void native_to_big_endian() noexcept;
    void big_to_native_endian() noexcept;
    void encode(boost::asio::mutable_buffer buf) const;
    void decode(boost::asio::const_buffer buf);
  };

  static_assert(sizeof(header) == header::SIZE);
  static_assert(sizeof(record) == record::SIZE);
  static_assert(sizeof(record_page) == record_page::SIZE);

  /**
   * \brief A page in the log.
   */
  class monsoon_tx_local_ page {
    friend log_entry;

    public:
    page() = default;
    page(std::uint64_t off);

    void decode(const txfile::transaction& tx, std::uint64_t off);
    void encode(txfile::transaction& tx) const;

    /**
     * \brief Test if the page is unused.
     * \details
     * A page is unused if there are no data locks held,
     * and no records other than skip-records.
     */
    auto unused() const noexcept -> bool;

    ///\brief Check if the page has space available.
    ///\param lck The lock on this page.
    auto space_avail(const std::unique_lock<std::mutex>& lck) const noexcept -> bool;

    ///\brief Allocate an entry.
    ///\param lck The lock on this page.
    auto new_entry(
        std::shared_ptr<txfile_allocator_log> owner,
        [[maybe_unused]] const std::unique_lock<std::mutex>& lck,
        txfile& f, action act, std::uint64_t addr, std::uint64_t len,
        allocator_type tx_allocator)
    -> std::shared_ptr<log_entry>;

    ///\brief Change the action of a given record.
    void write_action(
        std::shared_ptr<page> self,
        [[maybe_unused]] const std::unique_lock<std::mutex>& lck,
        txfile::transaction& tx, std::size_t idx, action act,
        tx_op_collection& ops);

    ///\brief Change the address and length of a given record.
    void write_addr_len(
        std::shared_ptr<page> self,
        [[maybe_unused]] const std::unique_lock<std::mutex>& lck,
        txfile::transaction& tx, std::size_t idx, std::uint64_t addr, std::uint64_t len,
        tx_op_collection& ops);

    ///\brief Acquire all active records.
    ///\details This is part of the maintenance logic, where all inactive records are gathered up so they can be stored in the tree instead.
    void maintenance(
        std::shared_ptr<txfile_allocator_log> owner,
        [[maybe_unused]] const std::unique_lock<std::mutex>& lck,
        std::back_insert_iterator<maintenance_result> out,
        allocator_type tx_allocator);

    auto next_page() const noexcept -> const std::uint64_t& { return rpage_.next_page; }
    auto next_page() noexcept -> std::uint64_t& { return rpage_.next_page; }

    private:
    auto offset_for_idx_(std::size_t idx) const noexcept {
      return off + offsetof(record_page, data) + idx * record::SIZE;
    }

    public:
    std::uint64_t off;
    mutable std::mutex mtx;

    private:
    std::size_t next_avail_slot_ = 0;
    std::bitset<record_page::DATA_NELEMS> data_locks_;
    record_page rpage_;
  };

  ///\brief List of the pages.
  ///\note The list must provide stable iterators.
  using page_list = std::list<
      page,
      std::allocator_traits<allocator_type>::rebind_alloc<page>>;

  public:
  /**
   * \brief A log entry for the file.
   * \details
   * A log entry contains one operation.
   *
   * It provides accessors for changing the operation.
   *
   * A log entry should only be used in a single transaction.
   */
  class log_entry
  : public std::enable_shared_from_this<log_entry>
  {
    friend page;

    public:
    log_entry() = default;

    log_entry(const log_entry&) = delete;
    log_entry(log_entry&&) = default;
    log_entry& operator=(const log_entry&) = delete;
    log_entry& operator=(log_entry&&) = default;
    ~log_entry() noexcept;

    ///\brief Change the action of this log entry.
    void on_commit(txfile::transaction& tx, action act, tx_op_collection& ops);
    ///\brief Change the byte range of this log entry.
    void modify_addr_len(txfile::transaction& tx, std::uint64_t addr, std::uint64_t len, tx_op_collection& ops);

    ///\brief Get the action of this record.
    auto get_action() const noexcept -> action { return act_; }
    ///\brief Get the address on which this record acts.
    auto get_addr() const noexcept -> std::uint64_t { return addr_; }
    ///\brief Get the length in bytes on which this record acts.
    auto get_len() const noexcept -> std::uint64_t { return len_; }

    private:
    std::shared_ptr<page> page_;
    std::size_t elem_idx_;
    action act_;
    std::uint64_t addr_, len_;
  };

  ///\brief Size in bytes that the log takes up.
  static constexpr std::size_t SIZE = header::SIZE;

  ///\brief Load an existing transaction log.
  ///\note Use `std::allocate_shared` because the log requires `shared_from_this`.
  txfile_allocator_log(const txfile::transaction& tx, std::uint64_t off, allocator_type allocator);
  ~txfile_allocator_log() noexcept;

  ///\brief Create a new log.
  ///\param tx Write transaction to create the log.
  ///\param off Offset at which to create the log.
  static void init(txfile::transaction& tx, std::uint64_t off);

  ///\brief Create a new entry with the given parameters.
  ///\param f The file in which the logs live.
  ///\param act Action to set on the new log entry.
  ///Use the `skip` action if you don't want the initial action to do anything.
  ///\param addr,len File block for the new log entry.
  ///\param page_allocator Callback for when the log needs to allocate a new block.
  ///This allocator is allowed to fail (with an empty optionable) in which case the log will grow the file instead.
  ///\param tx_allocator Allocator to control memory use for the transaction.
  ///\return A log entry with the specified data.
  auto new_entry(
      txfile& f,
      action act, std::uint64_t addr, std::uint64_t len,
      cheap_fn_ref<std::optional<std::uint64_t>(txfile::transaction&, std::uint64_t, tx_op_collection&)> page_allocator,
      allocator_type tx_allocator)
  -> std::shared_ptr<log_entry>;

  /**
   * \brief Perform maintenance.
   * \details
   * Internally scans the pages, trying to free any elements and pages.
   *
   * \param f The file in which the logs live.
   * \param page_allocator Callback for when the log needs to allocate a new block.
   * This allocator is allowed to fail (with an empty optionable) in which case the log will grow the file instead.
   * \param tx_allocator Allocator to control memory use for the transaction.
   * \return All log entries that can be cleaned up.
   */
  auto maintenance(
      txfile& f,
      cheap_fn_ref<std::optional<std::uint64_t>(txfile::transaction&, std::uint64_t, tx_op_collection&)> page_allocator,
      allocator_type tx_allocator)
  -> maintenance_result;

  ///\brief Create free space, by making the file larger.
  ///\param f The file in which the logs live, and that will be grown.
  ///\param page_allocator Callback for when the log needs to allocate a new block.
  ///This allocator is allowed to fail (with an empty optionable) in which case the log will grow the file instead.
  ///\param tx_allocator Allocator to control memory use for the transaction.
  ///\return A log entry describing the newly allocated space.
  auto allocate_by_growing_file(
      txfile& f,
      std::uint64_t bytes,
      cheap_fn_ref<std::optional<std::uint64_t>(txfile::transaction&, std::uint64_t, tx_op_collection&)> page_allocator,
      allocator_type tx_allocator)
  -> std::shared_ptr<log_entry>;

  private:
  ///\brief Create a new entry with the given parameters.
  ///\param lck Lock used for validating the correct lock is held.
  ///\param f The file in which the logs live.
  ///\param act Action to set on the new log entry.
  ///Use the `skip` action if you don't want the initial action to do anything.
  ///\param addr,len File block for the new log entry.
  ///\param page_allocator Callback for when the log needs to allocate a new block.
  ///This allocator is allowed to fail (with an empty optionable) in which case the log will grow the file instead.
  ///\param tx_allocator Allocator to control memory use for the transaction.
  ///\return A log entry with the specified data.
  monsoon_tx_local_ auto new_entry_(
      const std::unique_lock<std::mutex>& lck,
      txfile& f,
      action act, std::uint64_t addr, std::uint64_t len,
      cheap_fn_ref<std::optional<std::uint64_t>(txfile::transaction&, std::uint64_t, tx_op_collection&)> page_allocator,
      allocator_type tx_allocator)
  -> std::shared_ptr<log_entry>;

  ///\brief Appends a new, empty page to the pages.
  ///\param lck Lock used for validating the correct lock is held.
  ///\param f The file in which the logs live.
  ///\param page_allocator Callback for when the log needs to allocate a new block.
  ///This allocator is allowed to fail (with an empty optionable) in which case the log will grow the file instead.
  ///\param tx_allocator Allocator to control memory use for the transaction.
  monsoon_tx_local_ void append_new_page_(
      [[maybe_unused]] const std::unique_lock<std::mutex>& lck,
      txfile& f,
      cheap_fn_ref<std::optional<std::uint64_t>(txfile::transaction&, std::uint64_t, tx_op_collection&)> page_allocator,
      allocator_type tx_allocator);

  std::uint64_t off_;
  mutable std::mutex mtx_;
  page_list pages_;
};


} /* namespace monsoon::tx::detail */

#endif /* MONSOON_TX_DETAIL_TXFILE_ALLOCATOR_LOG_H */
