#include <monsoon/history/dir/io/tx_sequencer.h>

namespace monsoon::history::io {


tx_sequencer::tx::tx(tx_sequencer& seq)
: seq_(seq.weak_from_this()),
  record_(new record())
{
  boost::intrusive_ptr<record> tmp = record_;
  seq.c_.push_back(*tmp);
  tmp.detach();
}

tx_sequencer::tx::~tx() noexcept {
  if (record_ != nullptr) {
    const std::shared_ptr<tx_sequencer> seq_ptr = seq_.lock();
    if (seq_ptr != nullptr) {
      seq_ptr->c_.erase_and_dispose(seq_ptr->c_.iterator_to(*record_), &tx_sequencer::disposer_);
      seq_ptr->do_maintenance_();
    }
  }
}

auto tx_sequencer::tx::read_at(monsoon::io::fd::offset_type off, void* buf, std::size_t& nbytes) const -> std::size_t {
  const auto seq_ptr = std::shared_ptr<const tx_sequencer>(seq_);
  for (record_list::const_iterator iter = seq_ptr->c_.iterator_to(*record_);
      iter != seq_ptr->c_.end();
      ++iter) {
    if (iter->committed) {
      const std::size_t rlen = iter->replaced.read_at(off, buf, nbytes);
      if (rlen != 0) return rlen;
    }
  }
  return 0;
}

void tx_sequencer::tx::commit() {
  const auto seq_ptr = std::shared_ptr<tx_sequencer>(seq_);
  seq_ptr->c_.erase_and_dispose(seq_ptr->c_.iterator_to(*record_), &tx_sequencer::disposer_);
  record_->committed = true;
  seq_ptr->c_.push_back(*record_);
  record_.detach();
  seq_ptr->do_maintenance_();
}

void tx_sequencer::tx::record_previous_data_at(monsoon::io::fd::offset_type off, const void* buf, std::size_t nbytes) {
  return record_->replaced.write_at(off, buf, nbytes, false).commit();
}


tx_sequencer::~tx_sequencer() noexcept {
  c_.clear_and_dispose(&tx_sequencer::disposer_);
}

void tx_sequencer::do_maintenance_() noexcept {
  while (!c_.empty() && c_.front().committed)
    c_.pop_front_and_dispose(&tx_sequencer::disposer_);
}


} /* namespace monsoon::history::io */
