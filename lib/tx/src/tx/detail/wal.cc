#include <monsoon/tx/detail/wal.h>
#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>
#include <tuple>
#include <unordered_map>
#include <monsoon/xdr/xdr_stream.h>
#include <monsoon/io/positional_stream.h>
#include <monsoon/io/limited_stream.h>
#include <objpipe/of.h>
#include <monsoon/tx/instrumentation.h>

using namespace instrumentation::literals;

namespace monsoon::tx::detail {


struct wal_region::wal_header {
  static constexpr std::size_t XDR_SIZE = 12u;

  wal_header() noexcept = default;

  wal_header(std::uint32_t seq, std::uint64_t file_size) noexcept
  : file_size(file_size),
    seq(seq)
  {}

  void write(monsoon::xdr::xdr_ostream& out) const {
    out.put_uint32(seq);
    out.put_uint64(file_size);
  }

  static auto read(monsoon::xdr::xdr_istream& in) -> wal_header {
    const std::uint32_t seq = in.get_uint32();
    const std::uint64_t file_size = in.get_uint64();
    return wal_header(seq, file_size);
  }

  std::uint64_t file_size = 0u;
  std::uint32_t seq = 0u;
};


class wal_record_end
: public wal_record
{
  public:
  static constexpr std::size_t XDR_SIZE = 4u;

  wal_record_end() noexcept
  : wal_record(0u)
  {}

  ~wal_record_end() noexcept override = default;
  auto get_wal_entry() const noexcept -> wal_entry override { return wal_entry::end; }
  private:
  auto do_write([[maybe_unused]] monsoon::xdr::xdr_ostream& out) const -> void override {}
  auto do_apply([[maybe_unused]] wal_region& wal) const -> void override {}
};


class wal_record_commit
: public wal_record
{
  public:
  using wal_record::wal_record;

  ~wal_record_commit() noexcept override = default;
  auto get_wal_entry() const noexcept -> wal_entry override { return wal_entry::commit; }
  private:
  auto do_write([[maybe_unused]] monsoon::xdr::xdr_ostream& out) const -> void override {}
  auto do_apply([[maybe_unused]] wal_region& wal) const -> void override {}
};


class wal_record_write
: public wal_record
{
  public:
  wal_record_write() = delete;

  wal_record_write(tx_id_type tx_id, std::uint64_t offset, std::vector<uint8_t> data)
  : wal_record(tx_id),
    offset(offset),
    data(std::move(data))
  {}

  wal_record_write(tx_id_type tx_id, std::uint64_t offset, const void* buf, std::size_t len)
  : wal_record_write(
      tx_id,
      offset,
      std::vector<std::uint8_t>(
          reinterpret_cast<const std::uint8_t*>(buf),
          reinterpret_cast<const std::uint8_t*>(buf) + len))
  {}

  ~wal_record_write() noexcept override = default;

  static auto from_stream(tx_id_type tx_id, monsoon::xdr::xdr_istream& in)
  -> std::unique_ptr<wal_record_write> {
    const std::uint64_t offset = in.get_uint64();
    return std::make_unique<wal_record_write>(tx_id, offset, in.get_opaque());
  }

  auto get_wal_entry() const noexcept -> wal_entry override { return wal_entry::write; }

  static void to_stream(monsoon::xdr::xdr_ostream& out, tx_id_type tx_id, std::uint64_t offset, const void* buf, std::size_t len) {
    wal_record::to_stream(out, wal_entry::write, tx_id);
    to_stream_internal_(out, offset, buf, len);
  }

  private:
  static auto to_stream_internal_(monsoon::xdr::xdr_ostream& out, std::uint64_t offset, const void* buf, std::size_t len) -> void {
    out.put_uint64(offset);
    out.put_opaque(buf, len);
  }

  auto do_write(monsoon::xdr::xdr_ostream& out) const -> void override {
    to_stream_internal_(out, offset, data.data(), data.size());
  }

  auto do_apply(wal_region& wal) const -> void override {
    wal.repl_.write_at(offset, data.data(), data.size()).commit();
  }

  std::uint64_t offset;
  std::vector<std::uint8_t> data;
};


class wal_record_resize
: public wal_record
{
  public:
  wal_record_resize() = delete;

  wal_record_resize(tx_id_type tx_id, std::uint64_t new_size)
  : wal_record(tx_id),
    new_size(new_size)
  {}

  ~wal_record_resize() noexcept override = default;

  static auto from_stream(tx_id_type tx_id, monsoon::xdr::xdr_istream& in)
  -> std::unique_ptr<wal_record_resize> {
    return std::make_unique<wal_record_resize>(tx_id, in.get_uint64());
  }

  auto get_wal_entry() const noexcept -> wal_entry override { return wal_entry::resize; }

  private:
  auto do_write(monsoon::xdr::xdr_ostream& out) const -> void override {
    out.put_uint64(new_size);
  }

  auto do_apply(wal_region& wal) const -> void override {
    wal.fd_size_ = new_size;
  }

  std::uint64_t new_size;
};


wal_error::~wal_error() = default;

wal_bad_alloc::~wal_bad_alloc() = default;


wal_record::wal_record(tx_id_type tx_id)
: tx_id_(tx_id)
{
  if ((tx_id & tx_id_mask) != tx_id)
    throw std::logic_error("tx_id out of range (only 24 bit expected)");
}

wal_record::~wal_record() noexcept = default;

auto wal_record::read(monsoon::xdr::xdr_istream& in) -> std::unique_ptr<wal_record> {
  const std::uint32_t discriminant = in.get_uint32();
  std::unique_ptr<wal_record> result;
  const tx_id_type tx_id = discriminant >> 8;

  switch (discriminant & 0xff) {
    case static_cast<std::uint8_t>(wal_entry::end):
      if (tx_id != 0u) throw wal_error("unrecognized WAL entry");
      result = std::make_unique<wal_record_end>();
      break;
    case static_cast<std::uint8_t>(wal_entry::commit):
      result = std::make_unique<wal_record_commit>(tx_id);
      break;
    case static_cast<std::uint8_t>(wal_entry::write):
      result = wal_record_write::from_stream(tx_id, in);
      break;
    case static_cast<std::uint8_t>(wal_entry::resize):
      result = wal_record_resize::from_stream(tx_id, in);
      break;
    default:
      throw wal_error("unrecognized WAL entry");
  }

  assert(result != nullptr
      && static_cast<std::uint32_t>(result->get_wal_entry()) == discriminant);
  return result;
}

auto wal_record::write(monsoon::xdr::xdr_ostream& out) const -> void {
  assert((tx_id() & tx_id_mask) == tx_id());
  to_stream(out, get_wal_entry(), tx_id());
  do_write(out);
}

auto wal_record::to_stream(monsoon::xdr::xdr_ostream& out, wal_entry e, tx_id_type tx_id) -> void {
  out.put_uint32(static_cast<std::uint32_t>(e) | (tx_id << 8));
}

void wal_record::apply(wal_region& wal) const {
  do_apply(wal);
}

auto wal_record::is_end() const noexcept -> bool {
  return get_wal_entry() == wal_entry::end;
}

auto wal_record::is_commit() const noexcept -> bool {
  return get_wal_entry() == wal_entry::commit;
}

auto wal_record::is_control_record() const noexcept -> bool {
  const auto e = get_wal_entry();
  return e == wal_entry::end;
}

auto wal_record::make_end() -> std::unique_ptr<wal_record> {
  return std::make_unique<wal_record_end>();
}

auto wal_record::make_commit(tx_id_type tx_id) -> std::unique_ptr<wal_record> {
  return std::make_unique<wal_record_commit>(tx_id);
}

auto wal_record::make_write(tx_id_type tx_id, std::uint64_t offset, std::vector<uint8_t>&& data) -> std::unique_ptr<wal_record> {
  return std::make_unique<wal_record_write>(tx_id, offset, std::move(data));
}

auto wal_record::make_write(tx_id_type tx_id, std::uint64_t offset, const std::vector<uint8_t>& data) -> std::unique_ptr<wal_record> {
  return std::make_unique<wal_record_write>(tx_id, offset, data);
}

auto wal_record::make_resize(tx_id_type tx_id, std::uint64_t new_size) -> std::unique_ptr<wal_record> {
  return std::make_unique<wal_record_resize>(tx_id, new_size);
}


wal_region::wal_region(std::string name, monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len)
: off_(off),
  len_(len),
  fd_(std::move(fd)),
  instrumentation_grp_name_(std::move(name)),
  instrumentation_grp_(make_group("wal", monsoon_tx_instrumentation())["name"_tag = instrumentation_grp_name_]),
  commit_count_("commits", instrumentation_grp_),
  write_ops_("writes", instrumentation_grp_),
  compactions_("compactions", instrumentation_grp_),
  file_flush_("file_flush", instrumentation_grp_)
{
  static_assert(num_segments_ == 2u, "Algorithm assumes two segments.");
  std::array<std::tuple<std::size_t, wal_header>, 2> segments{
    std::make_tuple(0u, read_segment_header_(0)),
    std::make_tuple(1u, read_segment_header_(1))
  };
  std::sort(
      segments.begin(),
      segments.end(),
      [](const auto& x, const auto& y) -> bool {
        // Use a sliding window for sequencing distance.
        // Note: y.seq - x.seq may wrap-around, the comparison is based on that.
        return std::get<wal_header>(y).seq - std::get<wal_header>(x).seq <= 0x7fffffffu;
      });

  current_slot_ = std::get<std::size_t>(segments[0]);
  const auto old_slot = 1u - current_slot_;
  const auto old_data = read_segment_(old_slot);
  fd_size_ = old_data.file_size;
  current_seq_ = old_data.seq + 1u;

  // In-memory application of the WAL log.
  // Note that if the WAL log is bad, this will throw.
  {
    std::unordered_map<wal_record::tx_id_type, std::vector<const wal_record*>> records_by_tx;
    objpipe::of(std::cref(old_data.data))
        .iterate()
        .filter(
            [](const auto& record_ptr) -> bool {
              return !record_ptr->is_control_record();
            })
        .transform(
            [&records_by_tx](const auto& ptr) {
              std::vector<const wal_record*> result;
              if (ptr->is_commit()) {
                result.swap(records_by_tx[ptr->tx_id()]);
              } else {
                records_by_tx[ptr->tx_id()].push_back(ptr.get());
              }
              return result;
            })
        .iterate()
        .deref()
        .for_each(
            [this](const wal_record& record) {
              record.apply(*this);
            });
  }

  // If possible, recover the WAL log onto disk.
  if (fd_.can_write()) {
    using lp_writer = monsoon::io::limited_stream_writer<monsoon::io::positional_writer>;

    // Write all pending writes.
    for (const auto& w : repl_) {
      auto buf = reinterpret_cast<const std::uint8_t*>(w.data());
      auto len = w.size();
      auto off = w.begin_offset();
      while (len > 0) {
        const auto wlen = fd_.write_at(off + wal_end_offset(), buf, len);
        buf += wlen;
        len -= wlen;
        off += wlen;
      }
    }
    repl_.clear();
    fd_.truncate(monsoon::io::fd::size_type(wal_end_offset()) + fd_size_);
    fd_.flush(true);
    ++file_flush_;

    // Start a new segment.
    auto xdr = monsoon::xdr::xdr_stream_writer<lp_writer>(lp_writer(segment_len_(), fd_, slot_begin_off(current_slot_)));
    wal_header(current_seq_, fd_size_).write(xdr);
    slot_off_ = xdr.underlying_stream().offset();
    wal_record_end().write(xdr);

    // Flush data onto disk.
    fd_.flush(true);
    ++file_flush_;
  }
}

wal_region::wal_region(std::string name, [[maybe_unused]] create c, monsoon::io::fd&& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len)
: off_(off),
  len_(len),
  current_seq_(0),
  current_slot_(0),
  fd_(std::move(fd)),
  fd_size_(0),
  instrumentation_grp_name_(std::move(name)),
  instrumentation_grp_(make_group("wal", monsoon_tx_instrumentation())["name"_tag = instrumentation_grp_name_]),
  commit_count_("commits", instrumentation_grp_),
  write_ops_("writes", instrumentation_grp_),
  compactions_("compactions", instrumentation_grp_),
  file_flush_("file_flush", instrumentation_grp_)
{
  using lp_writer = monsoon::io::limited_stream_writer<monsoon::io::positional_writer>;

  if (fd_.size() < wal_end_offset()) fd_.truncate(wal_end_offset());

  const auto other_slot = 1u - current_slot_;

  {
    auto xdr = monsoon::xdr::xdr_stream_writer<lp_writer>(lp_writer(segment_len_(len_), fd_, slot_begin_off(current_slot_)));
    wal_header(0, 0).write(xdr);
    slot_off_ = xdr.underlying_stream().offset();
    wal_record_end().write(xdr);
  }

  {
    auto xdr = monsoon::xdr::xdr_stream_writer<lp_writer>(lp_writer(segment_len_(len_), fd_, slot_begin_off(other_slot)));
    wal_header(std::uint32_t(0) - 1u, 0).write(xdr);
    wal_record_end().write(xdr);
  }

  // While we only require a dataflush, we do a full flush to get the
  // file metadata synced up. Because it seems like a nice thing to do.
  fd_.flush();
  ++file_flush_;
}

auto wal_region::allocate_tx_id() -> wal_record::tx_id_type {
  std::unique_lock<std::mutex> lck{ alloc_mtx_ };

  // First, ensure there is space to allocate a transaction ID.
  while (tx_id_avail_.empty() && tx_id_states_.size() > wal_record::tx_id_mask) [[unlikely]] {
    // Check if there is even room to be created by compacting.
    if (tx_id_completed_count_ == 0) [[unlikely]]
      throw wal_bad_alloc("Ran out of WAL transaction IDs.");

    // Compact the WAL log by replaying it.
    // We must release the lock temporarily, hence why this part is in a loop.
    lck.unlock();
    compact_();
    lck.lock();
  }

  // First recycle used IDs.
  if (!tx_id_avail_.empty()) {
    const auto tx_id = tx_id_avail_.top();
    assert(tx_id < tx_id_states_.size());
    assert((tx_id & wal_record::tx_id_mask) == tx_id);
    tx_id_states_[tx_id] = true;
    tx_id_avail_.pop();
    return tx_id;
  }

  // Only allocate a new ID if there are none for recycling.
  const wal_record::tx_id_type tx_id = tx_id_states_.size();
  assert((tx_id & wal_record::tx_id_mask) == tx_id);
  tx_id_states_.push_back(true);
  return tx_id;
}

auto wal_region::read_segment_header_(std::size_t idx) const -> wal_header {
  assert(idx < num_segments_);

  using lp_reader = monsoon::io::limited_stream_reader<monsoon::io::positional_reader>;

  auto xdr_stream = monsoon::xdr::xdr_stream_reader<lp_reader>(
      lp_reader(segment_len_(), fd_, slot_begin_off(idx)));
  return wal_header::read(xdr_stream);
}

auto wal_region::read_segment_(std::size_t idx) const -> wal_vector {
  assert(idx < num_segments_);

  using lp_reader = monsoon::io::limited_stream_reader<monsoon::io::positional_reader>;

  auto xdr_stream = monsoon::xdr::xdr_stream_reader<lp_reader>(
      lp_reader(segment_len_(), fd_, slot_begin_off(idx)));
  const auto header = wal_header::read(xdr_stream);

  wal_vector result;
  result.slot = idx;
  result.seq = header.seq;
  result.file_size = header.file_size;
  do {
    result.data.push_back(wal_record::read(xdr_stream));
  } while (!result.data.back()->is_end());

  return result;
}

auto wal_region::read_at(monsoon::io::fd::offset_type off, void* buf, std::size_t len) const -> std::size_t {
  std::shared_lock<std::shared_mutex> lck{ mtx_ };
  return read_at_(off, buf, len);
}

void wal_region::compact() {
  std::lock_guard<std::mutex> lck{ log_mtx_ };
  compact_();
}

auto wal_region::size() const noexcept -> monsoon::io::fd::size_type {
  std::shared_lock<std::shared_mutex> lck{ mtx_ };
  return fd_size_;
}

auto wal_region::read_at_(monsoon::io::fd::offset_type off, void* buf, std::size_t len) const -> std::size_t {
  // Reads past the logic end of the file will fail.
  if (off >= fd_size_) return 0;
  // Clamp len, so we won't perform reads past-the-end.
  if (len > fd_size_ - off) len = fd_size_ - off;
  // Zero length reads are very easy.
  if (len == 0u) return 0u;

  // Try to read from the list of pending writes.
  const auto repl_rlen = repl_.read_at(off, buf, len); // May modify len.
  if (repl_rlen != 0u) return repl_rlen;
  assert(len != 0u);

  // We have to fall back to the file.
  const auto read_rlen = fd_.read_at(off + wal_end_offset(), buf, len);
  if (read_rlen != 0u) [[likely]] return read_rlen;
  assert(len != 0u);

  // If the file-read failed, it means the file is really smaller.
  // Pretend the file is zero-filled.
  assert(off + wal_end_offset() >= fd_.size());
  std::fill_n(reinterpret_cast<std::uint8_t*>(buf), len, std::uint8_t(0u));
  return len;
}

void wal_region::log_write_(const wal_record& r) {
  assert(!r.is_commit()); // Commit is special cased.

  monsoon::xdr::xdr_bytevector_ostream<> xdr;
  r.write(xdr);
  assert(xdr.size() >= wal_record_end::XDR_SIZE);
  wal_record_end().write(xdr);

  log_write_raw_(xdr);
}

void wal_region::log_write_raw_(const monsoon::xdr::xdr_bytevector_ostream<>& xdr) {
  std::lock_guard<std::mutex> lck{ log_mtx_ };

  assert(slot_begin_off(current_slot_) <= slot_off_ && slot_off_ < slot_end_off(current_slot_));

#ifndef NDEBUG // Assert that the slot offset contains a record-end-marker.
  {
    using lp_reader = monsoon::io::limited_stream_reader<monsoon::io::positional_reader>;

    assert(slot_end_off(current_slot_) - slot_off_ >= wal_record_end::XDR_SIZE);

    auto xdr_read = monsoon::xdr::xdr_stream_reader<lp_reader>(lp_reader(wal_record_end::XDR_SIZE, fd_, slot_off_));
    const auto last_record = wal_record::read(xdr_read);
    assert(last_record->is_end());
    assert(xdr_read.at_end());
  }
#endif

  // Run a compaction cycle if the log has insuficient space for the new data.
  if (slot_end_off(current_slot_) - slot_off_ < xdr.size()) {
    compact_();

    if (slot_end_off(current_slot_) - slot_off_ < xdr.size())
      throw wal_bad_alloc("no space in WAL");
  }

  // Perform write-ahead of the record.
  {
    auto buf = xdr.data() + wal_record_end::XDR_SIZE;
    auto len = xdr.size() - wal_record_end::XDR_SIZE;
    auto off = slot_off_ + wal_record_end::XDR_SIZE;
    while (len > 0u) {
      const auto wlen = fd_.write_at(off, buf, len);
      buf += wlen;
      len -= wlen;
      off += wlen;
    }
  }
  fd_.flush(true);
  ++file_flush_;

  // Write the marker of the record.
  {
    auto buf = xdr.data();
    auto len = wal_record_end::XDR_SIZE;
    auto off = slot_off_;
    while (len > 0u) {
      const auto wlen = fd_.write_at(off, buf, len);
      buf += wlen;
      len -= wlen;
      off += wlen;
    }
  }

  // Advance slot offset.
  slot_off_ += xdr.size() - wal_record_end::XDR_SIZE;

  ++write_ops_;
}

void wal_region::compact_() {
  {
    // Don't run a compaction if we know we won't free up any information.
    std::lock_guard<std::mutex> alloc_lck{ alloc_mtx_ };
    if (tx_id_completed_count_ == 0u) return;
  }

  ++compactions_;
  monsoon::xdr::xdr_bytevector_ostream<> wal_segment_header;
  wal_header(current_seq_ + 1u, fd_size_).write(wal_segment_header);

  using lp_writer = monsoon::io::limited_stream_writer<monsoon::io::positional_writer>;

  const auto new_slot = 1u - current_slot_;
  auto xdr = monsoon::xdr::xdr_stream_writer<lp_writer>(lp_writer(
          slot_end_off(new_slot) - slot_begin_off(new_slot) - wal_segment_header.size(),
          fd_,
          slot_begin_off(new_slot) + wal_segment_header.size()));

  // Copy all information for in-progress transactions.
  objpipe::of(std::move(read_segment_(current_slot_).data))
      .iterate()
      .deref()
      .filter( // Skip control records.
          [](const auto& record) -> bool {
            return !record.is_control_record();
          })
      .filter( // Only valid transaction IDs.
          [this](const auto& record) -> bool {
            return tx_id_states_.size() > record.tx_id();
          })
      .filter( // Only copy in-progress transactions.
          [this](const auto& record) -> bool {
            return tx_id_states_[record.tx_id()];
          })
      .for_each(
          [&xdr](const auto& record) {
            record.write(xdr);
          });
  // Record the end-of-log offset.
  const auto new_slot_off = xdr.underlying_stream().offset();
  // And write an end-of-log record.
  wal_record_end().write(xdr);

  // Apply the replacement map.
  {
    std::lock_guard<std::shared_mutex> lck{ mtx_ };
    for (const auto& r : repl_) {
      auto off = r.begin_offset();
      auto buf = reinterpret_cast<const std::uint8_t*>(r.data());
      auto len = r.size();
      while (len > 0) {
        const auto wlen = fd_.write_at(off + wal_end_offset(), buf, len);
        off += wlen;
        buf += wlen;
        len -= wlen;
      }
    }
    // Now that the replacement map is written out, we can clear it.
    repl_.clear();
  }

  // Ensure all data is on disk, before activating the segment.
  fd_.flush(true);
  ++file_flush_;

  // Now that all the writes from the old segment have been applied,
  // and all active transaction information has been copied,
  // we can finally activate this new segment.
  //
  // Note that we don't flush after this write.
  // We don't have to, as we know that all information in this log
  // is the same as the application of the previous log on top of
  // the (now updated) file contents.
  // Until a new commit happens, both logs are equivalent.
  {
    auto buf = wal_segment_header.data();
    auto len = wal_segment_header.size();
    auto off = slot_begin_off(new_slot);
    while (len > 0) {
      const auto wlen = fd_.write_at(off, buf, len);
      buf += wlen;
      len -= wlen;
      off += wlen;
    }
  }

  {
    std::lock_guard<std::mutex> alloc_lck{ alloc_mtx_ };
    // Update tx_id allocation state.
    while (!tx_id_states_.empty() && !tx_id_states_.back())
      tx_id_states_.pop_back();
    while (!tx_id_avail_.empty()) tx_id_avail_.pop(); // tx_id_avail_ lacks a clear() method.
    for (wal_record::tx_id_type tx_id = 0u; tx_id < tx_id_states_.size() && tx_id <= wal_record::tx_id_mask; ++tx_id) {
      if (!tx_id_states_[tx_id]) {
        try {
          tx_id_avail_.push(tx_id);
        } catch (const std::bad_alloc&) {
          // SKIP: ignore this exception
        }
      }
    }
  }

  // Update segment information.
  current_slot_ = new_slot;
  slot_off_ = new_slot_off;
  ++current_seq_;
  tx_id_completed_count_ = 0;
}

void wal_region::tx_write_(wal_record::tx_id_type tx_id, monsoon::io::fd::offset_type off, const void* buf, std::size_t len) {
  monsoon::xdr::xdr_bytevector_ostream<> xdr;
  wal_record_write::to_stream(xdr, tx_id, off, buf, len);
  assert(xdr.size() >= wal_record_end::XDR_SIZE);
  wal_record_end().write(xdr);

  log_write_raw_(xdr);
}

void wal_region::tx_resize_(wal_record::tx_id_type tx_id, monsoon::io::fd::size_type new_size) {
  if (new_size > std::numeric_limits<std::uint64_t>::max())
    throw std::overflow_error("wal_region::tx::resize");

  log_write_(wal_record_resize(tx_id, new_size));
}

void wal_region::tx_commit_(wal_record::tx_id_type tx_id, replacement_map&& writes, std::optional<monsoon::io::fd::size_type> new_file_size, std::function<void(replacement_map)> undo_op_fn) {
  // Create record of the commit.
  monsoon::xdr::xdr_bytevector_ostream<> xdr;
  wal_record_commit(tx_id).write(xdr);
  assert(xdr.size() >= wal_record_end::XDR_SIZE);
  wal_record_end().write(xdr);

  // Grab the WAL lock.
  std::lock_guard<std::mutex> log_lck{ log_mtx_ };
  assert(slot_begin_off(current_slot_) <= slot_off_ && slot_off_ < slot_end_off(current_slot_));

#ifndef NDEBUG // Assert that the slot offset contains a record-end-marker.
  {
    using lp_reader = monsoon::io::limited_stream_reader<monsoon::io::positional_reader>;

    assert(slot_end_off(current_slot_) - slot_off_ >= wal_record_end::XDR_SIZE);

    auto xdr_read = monsoon::xdr::xdr_stream_reader<lp_reader>(lp_reader(wal_record_end::XDR_SIZE, fd_, slot_off_));
    const auto last_record = wal_record::read(xdr_read);
    assert(last_record->is_end());
    assert(xdr_read.at_end());
  }
#endif

  // Run a compaction cycle if the log has insuficient space for the new data.
  if (slot_end_off(current_slot_) - slot_off_ < xdr.size()) {
    compact_();

    if (slot_end_off(current_slot_) - slot_off_ < xdr.size())
      throw wal_bad_alloc("no space in WAL");
  }

  // Grab the lock that protects against non-wal changes.
  std::lock_guard<std::shared_mutex> lck{ mtx_ };

  // Prepare a merging of the transactions in repl.
  // We must prepare this after acquiring the mutex mtx_,
  // but before the WAL record is written.
  // We write into a copy of repl that we can swap later.
  replacement_map new_repl = repl_;
  for (const auto& w : writes)
    new_repl.write_at(w.begin_offset(), w.data(), w.size()).commit();

  // Prepare the undo map.
  // This map holds all data overwritten by this transaction.
  replacement_map undo;
  for (const auto& w : writes) {
    const std::unique_ptr<std::uint8_t[]> buf = std::make_unique<std::uint8_t[]>(w.size());

    auto off = w.begin_offset();
    while (off < w.end_offset()) {
      std::size_t len = w.end_offset() - off;
      assert(len <= w.size());

      const auto rlen = repl_.read_at(off, buf.get(), len);
      if (rlen != 0u) {
        undo.write_at(off, buf.get(), rlen).commit();
      } else if (off >= fd_size_) {
        std::fill_n(buf.get(), len, std::uint8_t(0));
        undo.write_at(off, buf.get(), len).commit();
      } else {
        if (len > fd_size_ - off) len = fd_size_ - off;
        undo.write_at_from_file(off, fd_, off + wal_end_offset(), len).commit();
      }
      off += len;
    }
  }

  // Write everything but the record header.
  // By not writing the record header, the transaction itself looks as if
  // the commit hasn't happened, because there's a wal_record_end message.
  {
    auto buf = xdr.data() + wal_record_end::XDR_SIZE;
    auto len = xdr.size() - wal_record_end::XDR_SIZE;
    auto off = slot_off_ + wal_record_end::XDR_SIZE;
    while (len > 0u) {
      const auto wlen = fd_.write_at(off, buf, len);
      buf += wlen;
      len -= wlen;
      off += wlen;
    }
    fd_.flush(true);
    ++file_flush_;
  }

  // Grab the allocation lock.
  std::lock_guard<std::mutex> alloc_lck{ alloc_mtx_ };
  assert(tx_id < tx_id_states_.size());
  assert(tx_id_states_[tx_id]);

  // Write the marker of the record.
  {
    auto buf = xdr.data();
    auto len = wal_record_end::XDR_SIZE;
    auto off = slot_off_;
    while (len > 0u) {
      const auto wlen = fd_.write_at(off, buf, len);
      buf += wlen;
      len -= wlen;
      off += wlen;
    }

    // If this flush fails, we can't recover.
    // The commit has been written in full and any attempt to undo it
    // would likely run into the same error as the flush operation.
    // So we'll log it and silently continue.
    //
    // While we only require a dataflush, we do a full flush to get the
    // file metadata synced up. Because it seems like a nice thing to do.
    try {
      fd_.flush();
      ++file_flush_;
    } catch (const std::exception& e) {
      std::cerr << "Warning: failed to flush WAL log: " << e.what() << std::endl;
    } catch (...) {
      std::cerr << "Warning: failed to flush WAL log." << std::endl;
    }
  }

  // Now commit the change in repl_.
  swap(repl_, new_repl); // Never throws.
  // And update the tx_id_states_.
  tx_id_states_[tx_id] = false;
  ++tx_id_completed_count_;
  // Update the file size.
  if (new_file_size.has_value()) {
    fd_size_ = *new_file_size;
    repl_.truncate(fd_size_);
  }

  // Advance slot offset.
  slot_off_ += xdr.size() - wal_record_end::XDR_SIZE;

  undo_op_fn(std::move(undo));

  ++commit_count_;
}

void wal_region::tx_rollback_(wal_record::tx_id_type tx_id) noexcept {
  std::lock_guard<std::mutex> alloc_lck{ alloc_mtx_ };
  assert(tx_id < tx_id_states_.size());
  assert(tx_id_states_[tx_id]);

  tx_id_states_[tx_id] = false;
  ++tx_id_completed_count_;
}


wal_region::tx::tx(const std::shared_ptr<wal_region>& wal) noexcept
: wal_(wal)
{
  if (wal != nullptr)
    tx_id_ = wal->allocate_tx_id();
}

wal_region::tx::~tx() noexcept {
  rollback();
}

wal_region::tx::operator bool() const noexcept {
  return wal_.lock() != nullptr;
}

void wal_region::tx::write_at(monsoon::io::fd::offset_type off, const void* buf, std::size_t len) {
  const auto file_size = size();
  if (off > file_size || file_size - monsoon::io::fd::size_type(off) < len)
    throw std::length_error("write past end of file (based on local transaction resize)");

  auto writes_tx = writes_.write_at(off, buf, len);
  std::shared_ptr<wal_region>(wal_)->tx_write_(tx_id_, off, buf, len);
  writes_tx.commit();
}

void wal_region::tx::resize(monsoon::io::fd::size_type new_size) {
  std::shared_ptr<wal_region>(wal_)->tx_resize_(tx_id_, new_size);
  new_file_size_.emplace(new_size);
}

void wal_region::tx::commit(std::function<void(replacement_map)> undo_op_fn) {
  std::shared_ptr<wal_region>(wal_)->tx_commit_(tx_id_, std::move(writes_), new_file_size_, std::move(undo_op_fn));
  wal_.reset();
}

void wal_region::tx::commit() {
  commit([]([[maybe_unused]] replacement_map discard) {});
}

void wal_region::tx::rollback() noexcept {
  const auto wal = wal_.lock();
  if (wal != nullptr) wal->tx_rollback_(tx_id_);
  wal_.reset();
}

auto wal_region::tx::read_at(monsoon::io::fd::offset_type off, void* buf, std::size_t len) const -> std::size_t {
  return read_at(
      off, buf, len,
      []([[maybe_unused]] const auto& off, [[maybe_unused]] const auto& buf, [[maybe_unused]] const auto& len) -> std::size_t {
        return 0u;
      });
}

auto wal_region::tx::size() const -> monsoon::io::fd::size_type {
  if (new_file_size_.has_value()) return *new_file_size_;
  return std::shared_ptr<wal_region>(wal_)->size();
}


} /* namespace monsoon::tx::detail */
