#ifndef IO_WAL_H
#define IO_WAL_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>
#include <monsoon/io/fd.h>
#include <monsoon/io/stream.h>
#include <monsoon/xdr/xdr.h>

namespace monsoon::history::io {


class wal_error
: public std::runtime_error
{
  public:
  using std::runtime_error::runtime_error;
  ~wal_error();
};


enum class wal_entry : std::uint8_t {
  end = 0,
  commit = 1,
  invalidate_previous_wal = 2,
  write = 10,
  resize = 11,
  copy = 20
};

class wal_record {
  public:
  using tx_id_type = std::uint32_t;

  static constexpr tx_id_type tx_id_mask = 0xffffffu;

  wal_record() = delete;
  wal_record(tx_id_type tx_id);
  virtual ~wal_record() noexcept;
  virtual auto get_wal_entry() const noexcept -> wal_entry = 0;
  private:
  virtual auto do_write(monsoon::xdr::xdr_ostream& out) const -> void = 0;
  virtual auto do_apply(monsoon::io::fd& fd) const -> void = 0;
  public:
  static auto read(monsoon::xdr::xdr_istream& in) -> std::unique_ptr<wal_record>;
  void write(monsoon::xdr::xdr_ostream& out) const;
  void apply(monsoon::io::fd& fd) const;
  auto is_end() const noexcept -> bool;
  auto is_commit() const noexcept -> bool;
  auto tx_id() const noexcept -> std::uint32_t { return tx_id_; }

  static auto make_end() -> std::unique_ptr<wal_record>;
  static auto make_commit(tx_id_type tx_id) -> std::unique_ptr<wal_record>;
  static auto make_invalidate_previous_wal() -> std::unique_ptr<wal_record>;
  static auto make_write(tx_id_type tx_id, std::uint64_t offset, std::vector<uint8_t>&& data) -> std::unique_ptr<wal_record>;
  static auto make_write(tx_id_type tx_id, std::uint64_t offset, const std::vector<uint8_t>& data) -> std::unique_ptr<wal_record>;
  static auto make_resize(tx_id_type tx_id, std::uint64_t new_size) -> std::unique_ptr<wal_record>;
  static auto make_copy(tx_id_type tx_id, std::uint64_t src, std::uint64_t dst, std::uint64_t len) -> std::unique_ptr<wal_record>;

  private:
  tx_id_type tx_id_;
};


class wal_reader
: public monsoon::io::stream_reader
{
  public:
  wal_reader(const monsoon::io::fd& fd, monsoon::io::fd::offset_type off, monsoon::io::fd::size_type len)
  : fd_(fd),
    off_(off),
    len_(len)
  {}

  ~wal_reader() noexcept override;

  auto read(void* buf, std::size_t nbytes) -> std::size_t override;
  void close() override;
  auto at_end() const -> bool override;

  private:
  const monsoon::io::fd& fd_;
  monsoon::io::fd::offset_type off_;
  monsoon::io::fd::size_type len_;
};


} /* namespace monsoon::history::io */

#endif /* IO_WAL_H */
