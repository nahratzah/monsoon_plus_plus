#ifndef MONSOON_TX_DETAIL_TX_SEQUENCER_H
#define MONSOON_TX_DETAIL_TX_SEQUENCER_H

#include <monsoon/tx/detail/export_.h>
#include <atomic>
#include <cstdint>
#include <functional>
#include <shared_mutex>
#include <utility>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/options.hpp>
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>
#include <monsoon/tx/detail/replacement_map.h>

namespace monsoon::tx::detail {


/**
 * \brief The transaction sequencer keeps track of the order of transactions.
 * \details
 * The transaction sequencer can be used to figure out if a transaction was
 * started before another transaction was committed.
 */
class monsoon_tx_export_ tx_sequencer
: public std::enable_shared_from_this<tx_sequencer>
{
  private:
  struct record
  : boost::intrusive::list_base_hook<>,
    boost::intrusive_ref_counter<record>
  {
    bool committed = false;
    replacement_map replaced;
  };

  using record_list = boost::intrusive::list<
      record,
      boost::intrusive::constant_time_size<false>>;

  static auto disposer_(record* ptr) -> boost::intrusive_ptr<record> {
    return boost::intrusive_ptr<record>(ptr, false);
  }

  public:
  /**
   * \brief A transaction in the sequencer.
   * \details
   * The transaction exposes methods that operate on the sequencer.
   */
  class monsoon_tx_export_ tx {
    friend tx_sequencer;

    public:
    ///\brief Start a new transaction.
    ///\details
    ///Read operations on the returned transaction will be sequenced between
    ///all commits that have happened before, and before any commits that
    ///happened after this transaction was started.
    ///\param[in,out] seq The sequencer object.
    ///\param[in] cb Callback that will be invoked sequentially under the sequencer lock.
    ///\return The new transaction.
    template<typename CB>
    explicit tx(std::shared_ptr<tx_sequencer> seq, CB&& cb)
    : seq_(seq)
    {
      std::lock_guard<std::shared_mutex> lck{ seq->mtx_ };

      std::invoke(std::forward<CB>(cb));

      boost::intrusive_ptr<tx_sequencer::record> tmp = record_ = new record();
      seq->c_.push_back(*tmp);
      tmp.detach();
    }

    tx() = default;
    ~tx() noexcept;

    tx(const tx&) = delete;
    tx& operator=(const tx&) = delete;

    tx(tx&& y) noexcept
    : seq_(std::move(y.seq_)),
      record_()
    {
      record_.swap(y.record_);
    }

    tx& operator=(tx&& y) noexcept {
      if (&y != this) {
        seq_ = std::move(y.seq_);
        (record_ = nullptr).swap(y.record_);
      }
      return *this;
    }

    /**
     * \brief Perform a transaction-isolated read.
     * \details
     * This read operation explores the sequence of transactions
     * and uses the replacement_map in transactions written after,
     * to retrieve results from before those transactions were committed.
     *
     * If there is not replacement at the \p off, the function will clamp
     * \p nbytes appropriately and return 0.
     * \param[in] off The offset at which to read.
     * \param[out] buf The buffer into which to read.
     * \param[in,out] nbytes The number of bytes that is to be read.
     * \return Number of bytes read.
     */
    auto read_at(monsoon::io::fd::offset_type off, void* buf, std::size_t& nbytes) const -> std::size_t;
    /**
     * \brief Commit this transaction.
     * \details
     * Records that the transaction was committed and exposes the change set to
     * read operations.
     * \param[in] undo_map Map containing the inverse operation.
     */
    void commit(replacement_map&& undo_map) noexcept;

    private:
    std::weak_ptr<tx_sequencer> seq_;
    boost::intrusive_ptr<tx_sequencer::record> record_;
  };

  tx_sequencer() = default;

  tx_sequencer(const tx_sequencer&) = delete;
  tx_sequencer(tx_sequencer&&) = delete;
  tx_sequencer& operator=(const tx_sequencer&) = delete;
  tx_sequencer& operator=(tx_sequencer&&) = delete;

  ~tx_sequencer() noexcept;

  private:
  void do_maintenance_() noexcept;

  ///\brief Collection holding the records of the transactions.
  record_list c_;
  mutable std::shared_mutex mtx_;
};


} /* namespace monsoon::tx::detail */

#endif /* MONSOON_TX_DETAIL_TX_SEQUENCER_H */
