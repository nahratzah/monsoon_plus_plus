#include <monsoon/history/dir/io/tx_sequencer.h>

namespace monsoon::history::io {


tx_sequencer::tx::tx(tx_sequencer& seq)
: seq_(seq.shared_from_this()),
  record_(new struct record())
{
  boost::intrusive_ptr<tx_sequencer::record> tmp = record_;
  seq.c_.push_back(*tmp);
  tmp.detach();
}

tx_sequencer::tx::~tx() noexcept {
  if (record_ != nullptr && !record_->committed) {
    seq_->c_.erase_and_dispose(seq_->c_.iterator_to(*record_), &tx_sequencer::disposer_);
    seq_->do_maintenance_();
  }
}

auto tx_sequencer::tx::read_at(monsoon::io::fd::offset_type off, void* buf, std::size_t& nbytes) const -> std::size_t {
  for (record_list::const_iterator iter = seq_->c_.iterator_to(*record_);
      iter != seq_->c_.end();
      ++iter) {
    if (iter->committed) {
      const std::size_t rlen = iter->replaced.read_at(off, buf, nbytes);
      if (rlen != 0) return rlen;
    }
  }
  return 0;
}

void tx_sequencer::tx::commit() noexcept {
  seq_->c_.erase_and_dispose(seq_->c_.iterator_to(*record_), &tx_sequencer::disposer_);
  record_->committed = true;
  seq_->c_.push_back(*record_);
  record_.detach();
  seq_->do_maintenance_();
  seq_.reset();
}


tx_sequencer::~tx_sequencer() noexcept {
  c_.clear_and_dispose(&tx_sequencer::disposer_);
}

void tx_sequencer::do_maintenance_() noexcept {
  while (!c_.empty() && c_.front().committed)
    c_.pop_front_and_dispose(&tx_sequencer::disposer_);
}


} /* namespace monsoon::history::io */
