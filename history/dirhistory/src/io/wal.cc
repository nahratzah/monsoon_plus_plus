#include "wal.h"
#include <cassert>

namespace monsoon::history::io {
namespace {


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
  assert((tx_id & tx_id_mask) == tx_id);
  out.put_uint32(static_cast<std::uint32_t>(get_wal_entry()) | (tx_id() << 8));
  do_write(out);
}

void wal_record::apply(monsoon::io::fd& fd) const {
  do_apply(fd);
}

auto wal_record::is_end() const noexcept -> bool {
  return get_wal_entry() == wal_entry::end;
}

auto wal_record::is_commit() const noexcept -> bool {
  return get_wal_entry() == wal_entry::commit;
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
  if (len_ == 0) throw std::logic_error("end of WAL");

  if (nbytes > len_) nbytes = len_;
  const auto rlen = fd_.read_at(off_, buf, nbytes);
  assert(rlen <= len_);
  off_ += rlen;
  len_ -= rlen;
  return rlen;
}

void wal_reader::close() {}

auto wal_reader::at_end() const -> bool {
  return len_ == 0u;
}


} /* namespace monsoon::history::io */
