#include <monsoon/history/dir/io/tx_sequencer.h>
#include <tuple>

namespace monsoon::history::io {


auto tx_sequencer::is_read_before(const tx& read_tx, const tx& write_tx) const {
  const auto read_rank =  c_.project<lookup_idx>(c_.get<lookup>().find(std::make_tuple(read_tx.idx_, false)));
  const auto write_rank = c_.project<lookup_idx>(c_.get<lookup>().find(std::make_tuple(write_tx.idx_, true)));
  return read_rank < write_rank;
}

auto tx_sequencer::begin() -> tx {
  const auto seq = seq_.fetch_add(1u, std::memory_order_relaxed);
  c_.push_back(ordering{ seq, false });
  return tx(seq);
}

void tx_sequencer::commit(const tx& tx_object) {
  c_.get<lookup>().erase(c_.get<lookup>().find(std::make_tuple(tx_object.idx_, false)));
  c_.push_back(ordering{ tx_object.idx_, true });
}

void tx_sequencer::rollback(const tx& tx_object) {
  c_.get<lookup>().erase(c_.get<lookup>().find(std::make_tuple(tx_object.idx_, false)));
}


} /* namespace monsoon::history::io */
