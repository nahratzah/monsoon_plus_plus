#include <monsoon/tx/detail/tx_sequencer.h>

namespace monsoon::tx::detail {


tx_sequencer::tx::~tx() noexcept {
  if (record_ != nullptr && !record_->committed) {
    const auto seq = seq_.lock();
    if (seq != nullptr) {
      std::lock_guard<std::shared_mutex> lck{ seq->mtx_ };

      seq->c_.erase_and_dispose(seq->c_.iterator_to(*record_), &tx_sequencer::disposer_);
      seq->do_maintenance_();
    }
  }
}

auto tx_sequencer::tx::read_at(monsoon::io::fd::offset_type off, void* buf, std::size_t& nbytes) const -> std::size_t {
  const auto seq = std::shared_ptr<tx_sequencer>(seq_);
  std::shared_lock<std::shared_mutex> lck{ seq->mtx_ };

  for (record_list::const_iterator iter = seq->c_.iterator_to(*record_);
      iter != seq->c_.end();
      ++iter) {
    if (iter->committed) {
      const std::size_t rlen = iter->replaced.read_at(off, buf, nbytes);
      if (rlen != 0) return rlen;
    }
  }
  return 0;
}

void tx_sequencer::tx::commit(replacement_map&& undo_map) noexcept {
  const auto seq = std::shared_ptr<tx_sequencer>(seq_);
  std::lock_guard<std::shared_mutex> lck{ seq->mtx_ };

  seq->c_.erase_and_dispose(seq->c_.iterator_to(*record_), &tx_sequencer::disposer_);
  record_->replaced = std::move(undo_map);
  record_->committed = true;
  seq->c_.push_back(*record_);
  record_.detach();
  seq->do_maintenance_();

  seq_.reset();
}


tx_sequencer::~tx_sequencer() noexcept {
  c_.clear_and_dispose(&tx_sequencer::disposer_);
}

void tx_sequencer::do_maintenance_() noexcept {
  while (!c_.empty() && c_.front().committed)
    c_.pop_front_and_dispose(&tx_sequencer::disposer_);
}


} /* namespace monsoon::tx::detail */
