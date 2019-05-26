#include <monsoon/history/dir/io/wal.h>
#include <cassert>
#include <unordered_set>
#include <monsoon/xdr/xdr_stream.h>

namespace monsoon::history::io {
namespace {


auto prepare_undo_information(const monsoon::io::fd& fd, const wal_region& wal, replacement_map& undo_op, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len) -> std::vector<replacement_map::tx> {
  std::vector<replacement_map::tx> tx;

  assert(wal.wal_end_offset() <= off);
  while (len > 0u) {
    std::size_t bufsiz = 64u * 1024u * 1024u;
    if (bufsiz > len) bufsiz = len;
    tx.emplace_back(undo_op.write_at_from_file(off - wal.wal_end_offset(), fd, off, bufsiz, false));
    off += bufsiz;
    len -= bufsiz;
  }

  return tx;
}


class wal_record_end
: public wal_record
{
  public:
  wal_record_end() noexcept
  : wal_record(0u)
  {}

  ~wal_record_end() noexcept override = default;
  auto get_wal_entry() const noexcept -> wal_entry override { return wal_entry::end; }
  private:
  auto do_write([[maybe_unused]] monsoon::xdr::xdr_ostream& out) const -> void override {}
  auto do_apply([[maybe_unused]] monsoon::io::fd& fd) const -> void override {}
  auto do_apply([[maybe_unused]] monsoon::io::fd& fd, [[maybe_unused]] wal_region& wal, [[maybe_unused]] replacement_map& undo_op) const -> void override {}
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
  auto do_apply([[maybe_unused]] monsoon::io::fd& fd) const -> void override {}
  auto do_apply([[maybe_unused]] monsoon::io::fd& fd, [[maybe_unused]] wal_region& wal, [[maybe_unused]] replacement_map& undo_op) const -> void override {}
};


class wal_record_invalidate_previous_wal
: public wal_record
{
  public:
  wal_record_invalidate_previous_wal() noexcept
  : wal_record(0u)
  {}

  ~wal_record_invalidate_previous_wal() noexcept override = default;
  auto get_wal_entry() const noexcept -> wal_entry override { return wal_entry::invalidate_previous_wal; }
  private:
  auto do_write([[maybe_unused]] monsoon::xdr::xdr_ostream& out) const -> void override {}
  auto do_apply([[maybe_unused]] monsoon::io::fd& fd) const -> void override {}
  auto do_apply([[maybe_unused]] monsoon::io::fd& fd, [[maybe_unused]] wal_region& wal, [[maybe_unused]] replacement_map& undo_op) const -> void override {}
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

  ~wal_record_write() noexcept override = default;

  static auto from_stream(tx_id_type tx_id, monsoon::xdr::xdr_istream& in)
  -> std::unique_ptr<wal_record_write> {
    const std::uint64_t offset = in.get_uint64();
    return std::make_unique<wal_record_write>(tx_id, offset, in.get_opaque());
  }

  auto get_wal_entry() const noexcept -> wal_entry override { return wal_entry::write; }

  private:
  auto do_write(monsoon::xdr::xdr_ostream& out) const -> void override {
    out.put_uint64(offset);
    out.put_opaque(data);
  }

  auto do_apply(monsoon::io::fd& fd) const -> void override {
    const std::uint8_t* buf = data.data();
    auto len = data.size();
    auto off = offset;

    while (len > 0) {
      const auto wlen = fd.write_at(off, buf, len);
      buf += wlen;
      len -= wlen;
      off += wlen;
    }
  }

  auto do_apply([[maybe_unused]] monsoon::io::fd& fd, [[maybe_unused]] wal_region& wal, [[maybe_unused]] replacement_map& undo_op) const -> void override {
    assert(wal.wal_end_offset() <= offset);

    auto tx = undo_op.write_at_from_file(offset - wal.wal_end_offset(), fd, offset, data.size(), false);

    // Apply changes on the file.
    do_apply(fd);

    // Apply changes on the undo_op.
    tx.commit();
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

  auto do_apply(monsoon::io::fd& fd) const -> void override {
    fd.truncate(new_size);
  }

  auto do_apply([[maybe_unused]] monsoon::io::fd& fd, [[maybe_unused]] wal_region& wal, [[maybe_unused]] replacement_map& undo_op) const -> void override {
    assert(wal.wal_end_offset() <= new_size);

    std::vector<replacement_map::tx> tx_list;
    const auto old_size = fd.size();
    if (old_size > new_size) tx_list = prepare_undo_information(fd, wal, undo_op, new_size, old_size - new_size);

    do_apply(fd);

    for (auto& tx : tx_list) tx.commit();
  }

  std::uint64_t new_size;
};


class wal_record_copy
: public wal_record
{
  private:
  static constexpr std::uint64_t BUF_SIZE = 4u << 20;

  public:
  wal_record_copy() = delete;

  wal_record_copy(tx_id_type tx_id, std::uint64_t src, std::uint64_t dst, std::uint64_t len)
  : wal_record(tx_id),
    src(src),
    dst(dst),
    len(len)
  {}

  ~wal_record_copy() noexcept override = default;

  static auto from_stream(tx_id_type tx_id, monsoon::xdr::xdr_istream& in)
  -> std::unique_ptr<wal_record_copy> {
    const std::uint64_t src = in.get_uint64();
    const std::uint64_t dst = in.get_uint64();
    const std::uint64_t len = in.get_uint64();
    return std::make_unique<wal_record_copy>(tx_id, src, dst, len);
  }

  auto get_wal_entry() const noexcept -> wal_entry override { return wal_entry::copy; }

  private:
  auto do_write(monsoon::xdr::xdr_ostream& out) const -> void override {
    out.put_uint64(src);
    out.put_uint64(dst);
    out.put_uint64(len);
  }

  auto do_apply(monsoon::io::fd& fd) const -> void override {
    std::uint64_t src = this->src;
    std::uint64_t dst = this->dst;
    std::uint64_t len = this->len;

    std::vector<std::uint8_t> buf;
    if (len < BUF_SIZE)
      buf.resize(len);
    else
      buf.resize(BUF_SIZE);

    while (len > 0) {
      auto rlen = fd.read_at(src, buf.data(), std::min(len, buf.size()));

      while (rlen > 0) {
        const std::uint8_t* buf_ptr = buf.data();
        const auto wlen = fd.write_at(dst, buf_ptr, rlen);
        src += wlen;
        dst += wlen;
        len -= wlen;
        rlen -= wlen;
        buf_ptr += wlen;
      }
    }
  }

  auto do_apply([[maybe_unused]] monsoon::io::fd& fd, [[maybe_unused]] wal_region& wal, [[maybe_unused]] replacement_map& undo_op) const -> void override {
    assert(wal.wal_end_offset() <= dst);
    assert(wal.wal_end_offset() <= src);

    auto tx_list = prepare_undo_information(fd, wal, undo_op, dst, len);

    do_apply(fd);

    for (auto& tx : tx_list) tx.commit();
  }

  std::uint64_t src, dst, len;
};


} /* namespace monsoon::history::io::<unnamed> */


wal_error::~wal_error() = default;


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
    case static_cast<std::uint8_t>(wal_entry::invalidate_previous_wal):
      if (tx_id != 0u) throw wal_error("unrecognized WAL entry");
      result = std::make_unique<wal_record_invalidate_previous_wal>();
      break;
    case static_cast<std::uint8_t>(wal_entry::write):
      result = wal_record_write::from_stream(tx_id, in);
      break;
    case static_cast<std::uint8_t>(wal_entry::resize):
      result = wal_record_resize::from_stream(tx_id, in);
      break;
    case static_cast<std::uint8_t>(wal_entry::copy):
      result = wal_record_copy::from_stream(tx_id, in);
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
  out.put_uint32(static_cast<std::uint32_t>(get_wal_entry()) | (tx_id() << 8));
  do_write(out);
}

void wal_record::apply(monsoon::io::fd& fd) const {
  do_apply(fd);
}

void wal_record::apply(monsoon::io::fd& fd, wal_region& wal, replacement_map& undo) const {
  do_apply(fd, wal, undo);
}

auto wal_record::is_end() const noexcept -> bool {
  return get_wal_entry() == wal_entry::end;
}

auto wal_record::is_commit() const noexcept -> bool {
  return get_wal_entry() == wal_entry::commit;
}

auto wal_record::is_invalidate_previous_wal() const noexcept -> bool {
  return get_wal_entry() == wal_entry::invalidate_previous_wal;
}

auto wal_record::make_end() -> std::unique_ptr<wal_record> {
  return std::make_unique<wal_record_end>();
}

auto wal_record::make_commit(tx_id_type tx_id) -> std::unique_ptr<wal_record> {
  return std::make_unique<wal_record_commit>(tx_id);
}

auto wal_record::make_invalidate_previous_wal() -> std::unique_ptr<wal_record> {
  return std::make_unique<wal_record_invalidate_previous_wal>();
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

auto wal_record::make_copy(tx_id_type tx_id, std::uint64_t src, std::uint64_t dst, std::uint64_t len) -> std::unique_ptr<wal_record> {
  return std::make_unique<wal_record_copy>(tx_id, src, dst, len);
}


wal_reader::~wal_reader() noexcept = default;

auto wal_reader::read(void* buf, std::size_t nbytes) -> std::size_t {
  assert(fd_ != nullptr);
  if (len_ == 0) throw wal_error("corrupt WAL segment: too long");

  if (nbytes > len_) nbytes = len_;
  const auto rlen = fd_->read_at(off_, buf, nbytes);
  assert(rlen <= len_);
  off_ += rlen;
  len_ -= rlen;
  return rlen;
}

void wal_reader::close() {}

auto wal_reader::at_end() const -> bool {
  return len_ == 0u;
}


wal_region::wal_region(monsoon::io::fd& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len)
: off_(off),
  len_(len)
{
  static_assert(num_segments_ == 2u, "Algorithm assumes two segments.");
  std::array<wal_vector, num_segments_> segments{
    read_segment_(fd, 0),
    read_segment_(fd, 1)
  };

  std::sort(
      segments.begin(),
      segments.end(),
      [](const wal_vector& x, const wal_vector& y) -> bool {
        // Use a sliding window for sequencing distance.
        // Note: y.seq - x.seq may wrap-around, the comparison is based on that.
        return y.seq - x.seq <= 0x7fffffffu;
      });

  assert(std::all_of(
          segments.begin(), segments.end(),
          [](const wal_vector& v) {
            return !v.data.empty() && v.data.back()->is_end();
          }));

  const auto base_seq = segments.front().seq;
  std::for_each(segments.begin(), segments.end(), [base_seq](wal_vector& v) { v.seq -= base_seq; });

  if (!std::all_of(
          segments.begin(),
          segments.end(),
          [](const wal_vector& v) -> bool {
            return v.data.size() == 1u;
          })) {
    auto invalidation = segments.end();
    for (auto iter = segments.begin(); iter != segments.end(); ++iter) {
      if (std::any_of(
              iter->data.begin(),
              iter->data.end(),
              [](const auto& wal_record_ptr) -> bool {
                return wal_record_ptr->is_invalidate_previous_wal();
              }))
        invalidation = iter;
    }
    if (invalidation == segments.end())
      throw wal_error("unable to determine start of WAL");

    // Ensure we're operating on a sequential subset of entries.
    for (auto iter = invalidation; iter != segments.end() - 1; ++iter) {
      if (iter->seq + 1u != iter->seq)
        throw wal_error("missing WAL sequence IDs");
    }

    // Figure out the complete set of committed transactions.
    std::unordered_set<wal_record::tx_id_type> committed;
    for (auto iter = invalidation; iter != segments.end(); ++iter) {
      for (const auto& record_ptr : iter->data) {
        if (record_ptr->is_commit())
          committed.insert(record_ptr->tx_id());
      }
    }

    // Replay the WAL.
    for (auto iter = invalidation; iter != segments.end(); ++iter) {
      for (const auto& record_ptr : iter->data) {
        if (committed.count(record_ptr->tx_id()) > 0)
          record_ptr->apply(fd);
      }
    }

    // Sync state to disk.
    fd.flush();
  }

  // Start a new WAL entry that invalidates all previous entries.
  current_seq_ = base_seq + segments.back().seq + 1u;
  current_slot_ = segments.front().slot;
  const auto new_segment = make_empty_segment_(current_seq_, true);
  if (new_segment.size() > segment_len_()) throw wal_error("WAL segments too small");
  auto woff = off_ + segments.front().slot * segment_len_();
  auto buf = new_segment.data();
  auto buflen = new_segment.size();
  slot_off_ = buflen;
  while (buflen > 0) {
    const auto wlen = fd.write_at(woff, buf, buflen);
    woff += wlen;
    buf += wlen;
    buflen -= wlen;
  }
  slots_active_[segments.front().slot] = true;
  fd.flush(); // Sync new segment.

  assert(!slots_active_.all() && !slots_active_.none());
}

auto wal_region::create(monsoon::io::fd& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len) -> wal_region {
  const auto buf_vector = std::vector<std::uint8_t>(len, std::uint8_t(0));
  const std::uint8_t* buf = buf_vector.data();
  auto nbytes = buf_vector.size();
  auto woff = off;

  while (nbytes > 0) {
    const auto wlen = fd.write_at(woff, buf, nbytes);
    woff += wlen;
    buf += wlen;
    nbytes -= wlen;
  }

  fd.flush();
  return wal_region(fd, off, len);
}

auto wal_region::read_segment_(monsoon::io::fd& fd, std::size_t idx) -> wal_vector {
  assert(idx < num_segments_);

  wal_vector result;
  auto xdr_stream = monsoon::xdr::xdr_stream_reader<wal_reader>(
      wal_reader(fd, off_ + idx * segment_len_(), segment_len_()));

  result.slot = idx;
  result.seq = xdr_stream.get_uint32();
  do {
    result.data.push_back(wal_record::read(xdr_stream));
  } while (!result.data.back()->is_end());

  return result;
}

auto wal_region::make_empty_segment_(wal_seqno_type seq, bool invalidate) -> monsoon::xdr::xdr_bytevector_ostream<> {
  monsoon::xdr::xdr_bytevector_ostream<> x;

  x.put_uint32(seq);
  if (invalidate) wal_record::make_invalidate_previous_wal()->write(x);
  wal_record::make_end()->write(x);
  return x;
}


} /* namespace monsoon::history::io */
