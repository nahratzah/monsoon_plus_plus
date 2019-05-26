#ifndef MONSOON_HISTORY_DIR_IO_TX_SEQUENCER_H
#define MONSOON_HISTORY_DIR_IO_TX_SEQUENCER_H

#include <atomic>
#include <cstdint>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/options.hpp>
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>
#include <monsoon/history/dir/io/replacement_map.h>

namespace monsoon::history::io {


/**
 * \brief The transaction sequencer keeps track of the order of transactions.
 * \details
 * The transaction sequencer can be used to figure out if a transaction was
 * started before another transaction was committed.
 */
class tx_sequencer
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
  class tx {
    friend tx_sequencer;

    private:
    explicit tx(tx_sequencer& seq);

    public:
    tx() = default;
    ~tx() noexcept;

    auto read_at(monsoon::io::fd::offset_type off, void* buf, std::size_t& nbytes) const -> std::size_t;
    void commit();
    void record_previous_data_at(monsoon::io::fd::offset_type off, const void* buf, std::size_t nbytes);

    private:
    std::weak_ptr<tx_sequencer> seq_;
    boost::intrusive_ptr<record> record_;
  };

  tx_sequencer() = default;

  tx_sequencer(const tx_sequencer&) = delete;
  tx_sequencer(tx_sequencer&&) = delete;
  tx_sequencer& operator=(const tx_sequencer&) = delete;
  tx_sequencer& operator=(tx_sequencer&&) = delete;

  ~tx_sequencer() noexcept;

  auto begin() -> tx {
    return tx(*this);
  }

  private:
  record_list c_;
};


} /* namespace monsoon::history::io */

#endif /* MONSOON_HISTORY_DIR_IO_TX_SEQUENCER_H */
