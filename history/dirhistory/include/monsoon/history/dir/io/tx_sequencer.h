#ifndef MONSOON_HISTORY_DIR_IO_TX_SEQUENCER_H
#define MONSOON_HISTORY_DIR_IO_TX_SEQUENCER_H

#include <atomic>
#include <cstdint>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/tag.hpp>

namespace monsoon::history::io {


/**
 * \brief The transaction sequencer keeps track of the order of transactions.
 * \details
 * The transaction sequencer can be used to figure out if a transaction was
 * started before another transaction was committed.
 */
class tx_sequencer {
  public:
  class tx {
    friend tx_sequencer;

    private:
    explicit tx(std::uint32_t idx) : idx_(idx) {}

    private:
    std::uint32_t idx_;
  };

  private:
  struct ordering {
    std::uint32_t idx;
    bool is_write;
  };

  struct lookup_idx {};
  struct lookup {};

  using ordering_collection = boost::multi_index::multi_index_container<
      ordering,
      boost::multi_index::indexed_by<
          boost::multi_index::random_access<boost::multi_index::tag<lookup_idx>>,
          boost::multi_index::hashed_unique<
              boost::multi_index::tag<lookup>,
              boost::multi_index::composite_key<
                  ordering,
                  boost::multi_index::member<ordering, std::uint32_t, &ordering::idx>,
                  boost::multi_index::member<ordering, bool, &ordering::is_write>
                  >
              >
          >
      >;

  public:
  /**
   * \brief Test if a transaction began before another transaction was committed.
   * \details
   * When a transaction is started, each of its read operations must be
   * sequenced such that they only observe data at the moment they are begun.
   *
   * When a transaction is committed, it must appear unobservable to transactions
   * started before it.
   *
   * This function decides if the read-side of a transaction was started before
   * the other transaction was commited.
   */
  auto is_read_before(const tx& read_tx, const tx& write_tx) const;

  auto begin() -> tx;

  void commit(const tx& tx_object);

  void rollback(const tx& tx_object);

  private:
  ordering_collection c_;
  std::atomic<std::uint32_t> seq_{ 0u };
};


} /* namespace monsoon::history::io */

#endif /* MONSOON_HISTORY_DIR_IO_TX_SEQUENCER_H */
