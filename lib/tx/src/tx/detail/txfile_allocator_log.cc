#include <monsoon/tx/detail/txfile_allocator_log.h>
#include <monsoon/io/rw.h>
#include <algorithm>
#include <cassert>
#include <boost/endian/conversion.hpp>

namespace monsoon::tx::detail {


void txfile_allocator_log::header::native_to_big_endian() noexcept {
  boost::endian::native_to_big_inplace(magic);
  boost::endian::native_to_big_inplace(first_page);
}

void txfile_allocator_log::header::big_to_native_endian() noexcept {
  boost::endian::big_to_native_inplace(magic);
  boost::endian::big_to_native_inplace(first_page);
}


void txfile_allocator_log::record::native_to_big_endian() noexcept {
  boost::endian::native_to_big_inplace(act);
  for (auto& pad : pad0_) boost::endian::native_to_big_inplace(pad);
  boost::endian::native_to_big_inplace(addr);
  boost::endian::native_to_big_inplace(len);
}

void txfile_allocator_log::record::big_to_native_endian() noexcept {
  boost::endian::big_to_native_inplace(act);
  for (auto& pad : pad0_) boost::endian::big_to_native_inplace(pad);
  boost::endian::big_to_native_inplace(addr);
  boost::endian::big_to_native_inplace(len);
}

void txfile_allocator_log::record::decode(boost::asio::const_buffer buf) {
  boost::asio::buffer_copy(boost::asio::buffer(this, SIZE), buf);
  big_to_native_endian();
}

void txfile_allocator_log::record::encode(boost::asio::mutable_buffer buf) const {
  record copy = *this;
  copy.native_to_big_endian();
  boost::asio::buffer_copy(
      buf,
      boost::asio::buffer(&copy, SIZE));
}


void txfile_allocator_log::record_page::native_to_big_endian() noexcept {
  boost::endian::native_to_big_inplace(magic);
  boost::endian::native_to_big_inplace(next_page);
  for (auto& r : data) r.native_to_big_endian();
}

void txfile_allocator_log::record_page::big_to_native_endian() noexcept {
  boost::endian::big_to_native_inplace(magic);
  boost::endian::big_to_native_inplace(next_page);
  for (auto& r : data) r.big_to_native_endian();
}

void txfile_allocator_log::record_page::decode(boost::asio::const_buffer buf) {
  boost::asio::buffer_copy(boost::asio::buffer(this, SIZE), buf);
  big_to_native_endian();
  if (magic != record_page::MAGIC)
    throw std::runtime_error("bad magic value for txfile_allocator_log page");
}

void txfile_allocator_log::record_page::encode(boost::asio::mutable_buffer buf) const {
  auto copy = std::make_unique<record_page>(*this); // Use heap, because this is large.
  copy->native_to_big_endian();
  boost::asio::buffer_copy(buf, boost::asio::buffer(copy.get(), SIZE));
}


txfile_allocator_log::page::page(std::uint64_t off)
: off(off)
{
  std::memset(&rpage_, 0, sizeof(rpage_));
  rpage_.magic = record_page::MAGIC;

  assert(rpage_.next_page == 0);
  assert(unused());
}

void txfile_allocator_log::page::decode(const txfile::transaction& tx, std::uint64_t off) {
  this->off = off;
  data_locks_.reset();

  monsoon::io::read_at(tx, off, &rpage_, record_page::SIZE);
  rpage_.big_to_native_endian();
  if (rpage_.magic != record_page::MAGIC)
    throw std::runtime_error("bad magic value for txfile_allocator_log page");

  next_avail_slot_ = data_locks_.size();
  while (next_avail_slot_ > 0
      && action(rpage_.data[next_avail_slot_ - 1u].act) == action::skip)
    --next_avail_slot_;
}

void txfile_allocator_log::page::encode(txfile::transaction& tx) const {
  auto copy = std::make_unique<record_page>(rpage_); // Use heap, because this is large.
  copy->native_to_big_endian();
  monsoon::io::write_at(tx, off, copy.get(), record_page::SIZE);
}

auto txfile_allocator_log::page::unused() const noexcept -> bool {
  std::lock_guard<std::mutex> lck{ mtx };

  return data_locks_.none()
      && std::all_of(
          std::cbegin(rpage_.data), std::cend(rpage_.data),
          [](const record& r) -> bool {
            return r.get_action() == action::skip;
          });
}

auto txfile_allocator_log::page::space_avail([[maybe_unused]] const std::unique_lock<std::mutex>& lck) const noexcept -> bool {
  assert(lck.owns_lock() && lck.mutex() == &mtx);

  return next_avail_slot_ < record_page::DATA_NELEMS;
}

auto txfile_allocator_log::page::new_entry(
    std::shared_ptr<txfile_allocator_log> owner,
    [[maybe_unused]] const std::unique_lock<std::mutex>& lck,
    txfile& f, action act, std::uint64_t addr, std::uint64_t len,
    allocator_type tx_allocator)
-> std::shared_ptr<log_entry> {
  assert(lck.owns_lock() && lck.mutex() == &mtx);
  assert(next_avail_slot_ < record_page::DATA_NELEMS);

  // Prepare a log record that holds the lock for this entry.
  auto r = std::allocate_shared<log_entry>(tx_allocator);
  const auto idx = r->elem_idx_ = next_avail_slot_++;
  r->page_ = std::shared_ptr<page>(owner, this);
  data_locks_.set(idx);

  // Set the state properties.
  r->act_ = act;
  r->addr_ = addr;
  r->len_ = len;

  // Prepare transaction.
  auto tx = f.begin(false);
  tx_op_collection ops(tx_allocator);

  // Update the record.
  rpage_.data[idx].act = static_cast<std::underlying_type_t<action>>(act);
  rpage_.data[idx].addr = addr;
  rpage_.data[idx].len = len;
  ops.on_rollback(
      [this, idx]() {
        rpage_.data[idx].act = static_cast<std::underlying_type_t<action>>(action::skip);
        rpage_.data[idx].addr = 0;
        rpage_.data[idx].len = 0;
      });

  // Write the record.
  std::array<std::uint8_t, record::SIZE> buf;
  rpage_.data[idx].encode(boost::asio::buffer(buf));
  monsoon::io::write_at(tx, offset_for_idx_(idx), buf.data(), buf.size());

  tx.commit();
  ops.commit(); // Never throws.

  return r;
}

void txfile_allocator_log::page::write_action(
    std::shared_ptr<page> self,
    [[maybe_unused]] const std::unique_lock<std::mutex>& lck,
    txfile::transaction& tx, std::size_t idx, action act,
    tx_op_collection& ops) {
  assert(lck.owns_lock() && lck.mutex() == &mtx);
  assert(data_locks_[idx] == true);

  const std::underlying_type_t<action> act_be =
      boost::endian::native_to_big(static_cast<std::underlying_type_t<action>>(act));

  // Perform the write.
  monsoon::io::write_at(tx, offset_for_idx_(idx) + offsetof(record, act),
      &act_be, sizeof(act_be));
  // And update the in-memory state on commit.
  ops.on_commit(
      [self=std::move(self), idx, act]() {
        std::lock_guard<std::mutex> pg_lck{ self->mtx };
        self->rpage_.data[idx].act = static_cast<std::underlying_type_t<action>>(act);
      });
}

void txfile_allocator_log::page::write_addr_len(
    std::shared_ptr<page> self,
    [[maybe_unused]] const std::unique_lock<std::mutex>& lck,
    txfile::transaction& tx, std::size_t idx, std::uint64_t addr, std::uint64_t len,
    tx_op_collection& ops) {
  assert(lck.owns_lock() && lck.mutex() == &mtx);
  assert(data_locks_[idx] == true);

  // We require that addr and len are adjecent.
  static_assert(offsetof(record, addr) + sizeof(std::uint64_t) == offsetof(record, len));

  struct addr_and_len_t {
    std::uint64_t addr;
    std::uint64_t len;
  };

  addr_and_len_t addr_and_len{
    boost::endian::native_to_big(addr),
    boost::endian::native_to_big(len)
  };

  // Perform the write.
  monsoon::io::write_at(tx, offset_for_idx_(idx) + offsetof(record, addr),
      &addr_and_len, sizeof(addr_and_len));
  // And update the in-memory state on commit.
  ops.on_commit(
      [self=std::move(self), idx, addr, len]() {
        std::lock_guard<std::mutex> pg_lck{ self->mtx };
        self->rpage_.data[idx].addr = addr;
        self->rpage_.data[idx].len = len;
      });
}

void txfile_allocator_log::page::maintenance(
    std::shared_ptr<txfile_allocator_log> owner,
    [[maybe_unused]] const std::unique_lock<std::mutex>& lck,
    std::back_insert_iterator<maintenance_result> out,
    allocator_type tx_allocator) {
  assert(lck.owns_lock() && lck.mutex() == &mtx);

  auto record_iter = std::begin(rpage_.data);
  for (std::size_t idx = 0; idx < next_avail_slot_; ++idx, ++record_iter) {
    assert(record_iter != std::end(rpage_.data));

    // Skip locked datums.
    if (data_locks_[idx]) continue;
    // Skip skip-records.
    if (action(record_iter->act) == action::skip) continue;

    // Allocate log record.
    auto r = std::allocate_shared<log_entry>(tx_allocator);
    r->elem_idx_ = idx;
    r->page_ = std::shared_ptr<page>(owner, this);
    data_locks_.set(idx);

    // Fill in record parameters.
    r->act_ = record_iter->get_action();
    r->addr_ = record_iter->addr;
    r->len_ = record_iter->len;

    // Publish log record for maintenance.
    ++out = std::move(r);
  }
}


txfile_allocator_log::log_entry::~log_entry() noexcept {
  if (page_ == nullptr) return;

  std::unique_lock<std::mutex> pg_lck{ page_->mtx };
  assert(page_->data_locks_[elem_idx_] == true);
  page_->data_locks_.reset(elem_idx_);
}

void txfile_allocator_log::log_entry::on_commit(txfile::transaction& tx, action act, tx_op_collection& ops) {
  assert(page_ != nullptr);

  // We need to keep this log entry live until all the commits complete.
  // We do this by wrapping the page in a pointer that uses our liveness.
  auto page_ptr_with_log_entry_liveness = std::shared_ptr<page>(shared_from_this(), page_.get());

  std::unique_lock<std::mutex> pg_lck{ page_->mtx };
  page_->write_action(
      std::move(page_ptr_with_log_entry_liveness),
      pg_lck, tx, elem_idx_, act,
      ops);

  act_ = act;
}

void txfile_allocator_log::log_entry::modify_addr_len(txfile::transaction& tx, std::uint64_t addr, std::uint64_t len, tx_op_collection& ops) {
  assert(page_ != nullptr);

  // We need to keep this log entry live until all the commits complete.
  // We do this by wrapping the page in a pointer that uses our liveness.
  auto page_ptr_with_log_entry_liveness = std::shared_ptr<page>(shared_from_this(), page_.get());

  std::unique_lock<std::mutex> pg_lck{ page_->mtx };
  page_->write_addr_len(
      std::move(page_ptr_with_log_entry_liveness),
      pg_lck, tx, elem_idx_, addr, len,
      ops);

  addr_ = addr;
  len_ = len;
}


txfile_allocator_log::txfile_allocator_log(const txfile::transaction& tx, std::uint64_t off, allocator_type allocator)
: off_(off),
  pages_(allocator)
{
  // Decode header.
  header h;
  monsoon::io::read_at(tx, off, &h, header::SIZE);
  h.big_to_native_endian();
  if (h.magic != header::MAGIC)
    throw std::runtime_error("bad magic value for txfile_allocator_log");

  for (std::uint64_t pg_off = h.first_page;
      pg_off != 0u;
      pg_off = pages_.back().next_page()) {
    pages_.emplace_back();
    pages_.back().decode(tx, pg_off);
  }
}

txfile_allocator_log::~txfile_allocator_log() noexcept = default;

void txfile_allocator_log::init(txfile::transaction& tx, std::uint64_t off) {
  header h;
  h.magic = header::MAGIC;
  h.first_page = 0;
  h.native_to_big_endian();

  monsoon::io::write_at(tx, off, &h, header::SIZE);
}

auto txfile_allocator_log::new_entry(
    txfile& f, action act, std::uint64_t addr, std::uint64_t len,
    cheap_fn_ref<std::optional<std::uint64_t>(txfile::transaction&, std::uint64_t, tx_op_collection&)> page_allocator,
    allocator_type tx_allocator)
-> std::shared_ptr<log_entry> {
  std::unique_lock<std::mutex> lck{ mtx_ };
  return new_entry_(lck, f, act, addr, len, page_allocator, std::move(tx_allocator));
}

auto txfile_allocator_log::maintenance(
    txfile& f,
    cheap_fn_ref<std::optional<std::uint64_t>(txfile::transaction&, std::uint64_t, tx_op_collection&)> page_allocator,
    allocator_type tx_allocator)
-> maintenance_result {
  maintenance_result entries(tx_allocator);
  std::unique_lock<std::mutex> lck{ mtx_ };

  // Empty pages at the front are converted to free memory.
  while (pages_.size() > 1u) {
    // We must test if the page is unused, but we must do so while holding the lock.
    // Otherwise external records can modify the page while we check.
    std::unique_lock<std::mutex> pg_lck{ pages_.front().mtx };
    if (!pages_.front().unused()) break;

    // Create an empty log entry for this page.
    // We create with the skip setting, so we can change the action
    // in one big transaction.
    const auto pg_entry = new_entry_(lck, f,
        action::skip, pages_.front().off, record_page::SIZE,
        page_allocator,
        tx_allocator);

    // Initialize transaction logic.
    auto tx = f.begin(false);
    tx_op_collection ops(tx_allocator);

    // When this commits, the front page will have been freed.
    ops.on_commit(
        [this]() {
          pages_.pop_front();
        });
    // When this completes, the memory of this page is marked as free.
    pg_entry->on_commit(tx, action::free, ops);

    // Update offset for first page.
    {
      std::unique_lock<std::mutex> second_pg_lck{ std::next(pages_.begin())->mtx };
      const std::uint64_t new_first_page_be = boost::endian::native_to_big(std::next(pages_.begin())->off);
      monsoon::io::write_at(tx, off_ + offsetof(header, first_page), &new_first_page_be, sizeof(new_first_page_be));
    }

    // Release the page prior to commiting, so we don't try to destroy a locked mutex.
    pg_lck.unlock();

    // Commit the change.
    tx.commit();
    ops.commit(); // Never throws.
  }

  // Ask each page for elements that can be cleaned up.
  for (auto& pg : pages_) {
    std::unique_lock<std::mutex> pg_lck{ pg.mtx };
    pg.maintenance(shared_from_this(), pg_lck, std::back_inserter(entries), tx_allocator);
  }

  return entries;
}

auto txfile_allocator_log::allocate_by_growing_file(
    txfile& f,
    std::uint64_t bytes,
    cheap_fn_ref<std::optional<std::uint64_t>(txfile::transaction&, std::uint64_t, tx_op_collection&)> page_allocator,
    allocator_type tx_allocator)
-> std::shared_ptr<log_entry> {
  std::unique_lock<std::mutex> lck{ mtx_ };

  // Allocate an entry before growing the file.
  // This is necessary because the act of creating an entry may also grow the file.
  auto spc_entry = new_entry_(lck, f, action::skip, 0, 0, page_allocator, tx_allocator);

  // Initialize commit logic.
  auto tx = f.begin(false);
  tx_op_collection ops(tx_allocator);

  // Update the addr/len parameters of the entry.
  // Note that the entry is still of the skip type.
  spc_entry->modify_addr_len(tx, tx.size(), bytes, ops);
  // Grow the file.
  tx.resize(tx.size() + bytes);
  // Mark the entry as containing free space.
  spc_entry->on_commit(tx, action::free, ops);

  // File is grown, entry marks the growth as free space.
  // We can commit.
  tx.commit();
  ops.commit(); // Never throws.

  // Return the entry.
  return spc_entry;
}

auto txfile_allocator_log::new_entry_(
    const std::unique_lock<std::mutex>& lck,
    txfile& f, action act, std::uint64_t addr, std::uint64_t len,
    cheap_fn_ref<std::optional<std::uint64_t>(txfile::transaction&, std::uint64_t, tx_op_collection&)> page_allocator,
    allocator_type tx_allocator)
-> std::shared_ptr<log_entry> {
  assert(lck.owns_lock() && lck.mutex() == &mtx_);

  auto tx = f.begin(false);
  tx_op_collection ops(tx_allocator);

  // Ensure there is always a page.
  if (pages_.empty())
    append_new_page_(lck, f, page_allocator, tx_allocator);
  assert(!pages_.empty());

  // Ensure the back-page has space available.
  std::unique_lock<std::mutex> pg_lck;
  if (!pages_.empty()) {
    pg_lck = std::unique_lock<std::mutex>(pages_.back().mtx);
    if (!pages_.back().space_avail(pg_lck)) {
      // Create a new page and use that.
      append_new_page_(lck, f, page_allocator, tx_allocator);
      pg_lck = std::unique_lock<std::mutex>(pages_.back().mtx);
    }
  }

  assert(pg_lck.owns_lock() && pg_lck.mutex() == &pages_.back().mtx);
  assert(pages_.back().space_avail(pg_lck));

  return pages_.back().new_entry(
      shared_from_this(),
      lck,
      f, act, addr, len,
      std::move(tx_allocator));
}

void txfile_allocator_log::append_new_page_(
    [[maybe_unused]] const std::unique_lock<std::mutex>& lck,
    txfile& f,
    cheap_fn_ref<std::optional<std::uint64_t>(txfile::transaction&, std::uint64_t, tx_op_collection&)> page_allocator,
    allocator_type tx_allocator) {
  assert(lck.owns_lock() && lck.mutex() == &mtx_);

  // Initialize transaction logic.
  tx_op_collection ops(tx_allocator);
  auto tx = f.begin(false);

  // Allocate space for a new page.
  std::optional<std::uint64_t> opt_addr = page_allocator(tx, record_page::SIZE, ops);
  if (!opt_addr.has_value()) {
    // If the allocator can't provide space, grow the file.
    opt_addr.emplace(tx.size());
    tx.resize(*opt_addr + record_page::SIZE);
  }

  assert(opt_addr.has_value());

  // Write offsets.
  {
    const std::uint64_t new_page_off_be = boost::endian::native_to_big(*opt_addr);
    if (pages_.empty()) {
      monsoon::io::write_at(tx, off_ + offsetof(header, first_page), &new_page_off_be, sizeof(new_page_off_be));
    } else {
      monsoon::io::write_at(tx, pages_.back().off + offsetof(record_page, next_page), &new_page_off_be, sizeof(new_page_off_be));
      ops.on_commit(
          [pg=&pages_.back(), new_page_off=*opt_addr]() {
            pg->next_page() = new_page_off;
          });
    }
  }

  // Create a new page.
  pages_.emplace_back(*opt_addr);
  ops.on_rollback(
      [this]() {
        pages_.pop_back();
      });
  // Serialize new page to disk.
  pages_.back().encode(tx);

  tx.commit();
  ops.commit(); // Never throws.
}


} /* namespace monsoon::tx::detail */
