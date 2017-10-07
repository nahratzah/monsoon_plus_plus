#ifndef V2_ENCDEC_H
#define V2_ENCDEC_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <monsoon/xdr/xdr.h>
#include <monsoon/xdr/xdr_stream.h>
#include <monsoon/time_point.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>
#include <unordered_map>
#include <monsoon/histogram.h>
#include <monsoon/tags.h>
#include <monsoon/group_name.h>
#include <monsoon/metric_value.h>
#include <monsoon/time_series_value.h>
#include <monsoon/io/fd.h>
#include <monsoon/io/ptr_stream.h>
#include <monsoon/io/stream.h>
#include <mutex>
#include <memory>

namespace monsoon {
namespace history {
namespace v2 {
namespace header_flags {


monsoon_dirhistory_local_
constexpr std::uint32_t /* KIND: indicate if the type of file data. */
                        KIND_MASK        = 0x0000000f,
                        KIND_LIST        = 0x00000000,
                        KIND_TABLES      = 0x00000001,
                        /* Indicates segment compression algorithm. */
                        COMPRESSION_MASK = 0x3f000000,
                        LZO_1X1          = 0x10000000,
                        GZIP             = 0x20000000,
                        SNAPPY           = 0x30000000,
                        /* Set if the file has sorted/unique timestamps. */
                        SORTED           = 0x40000000,
                        DISTINCT         = 0x80000000;


} /* namespace monsoon::history::v2::header_flags */


class monsoon_dirhistory_local_ file_segment_ptr {
 public:
  using offset_type = io::fd::offset_type;
  using size_type = io::fd::size_type;

  file_segment_ptr() = default;
  file_segment_ptr(const file_segment_ptr&) = default;
  file_segment_ptr& operator=(const file_segment_ptr&) = default;
  file_segment_ptr(offset_type, size_type) noexcept;

  offset_type offset() const noexcept { return off_; }
  size_type size() const noexcept { return len_; }

 private:
  offset_type off_;
  size_type len_;
};

class monsoon_dirhistory_local_ encdec_ctx {
 public:
  enum class compression_type : std::uint32_t {
    NONE = 0,
    LZO_1X1 = header_flags::LZO_1X1,
    GZIP = header_flags::GZIP,
    SNAPPY = header_flags::SNAPPY
  };

  encdec_ctx() noexcept;
  encdec_ctx(const encdec_ctx&);
  encdec_ctx(encdec_ctx&&) noexcept;
  encdec_ctx& operator=(const encdec_ctx&);
  encdec_ctx& operator=(encdec_ctx&&) noexcept;
  encdec_ctx(std::shared_ptr<io::fd>, std::uint32_t) noexcept;
  ~encdec_ctx() noexcept;

  const std::shared_ptr<io::fd>& fd() const noexcept { return fd_; }
  std::uint32_t flags() const noexcept { return hdr_flags_; }
  auto new_reader(const file_segment_ptr&, bool = true) const
      -> xdr::xdr_stream_reader<io::ptr_stream_reader>;
  auto decompress_(io::ptr_stream_reader&&, bool) const
      -> std::unique_ptr<io::stream_reader>;

  compression_type compression() const noexcept {
    return compression_type(hdr_flags_ & header_flags::COMPRESSION_MASK);
  }

 private:
  std::shared_ptr<io::fd> fd_ = nullptr;
  std::uint32_t hdr_flags_ = 0;
};

/*
 * Pointer to file segment.
 *
 * A file segment is a block in the file.
 * It starts at the given 'offset' (bytes from begin for file).
 * The file segment contains 'len' bytes of data.
 * If the compress bit is specified, this data will be the length after compression.
 *
 * Following the data, between 0 and 3 padding bytes will exist, such that:
 *   (padlen + 'len') % 4 == 0
 *
 * After the padding, a 4 byte CRC32 is written in BIG ENDIAN (xdr int).
 * The CRC32 is calculated over the data and the padding bytes.
 */
template<typename T>
class monsoon_dirhistory_local_ file_segment {
 public:
  using offset_type = file_segment_ptr::offset_type;
  using size_type = file_segment_ptr::size_type;

  file_segment() noexcept;
  file_segment(const file_segment&) = delete;
  file_segment(file_segment&&) noexcept;
  file_segment& operator=(const file_segment&) = delete;
  file_segment& operator=(file_segment&&) noexcept;
  file_segment(const encdec_ctx&, file_segment_ptr,
      std::function<std::shared_ptr<T> (xdr::xdr_istream&)>&&, bool = true) noexcept;

  std::shared_ptr<const T> get() const;
  const encdec_ctx& ctx() const noexcept { return ctx_; }
  const file_segment_ptr& file_ptr() const { return ptr_; }

 private:
  file_segment_ptr ptr_;
  encdec_ctx ctx_;
  std::function<std::shared_ptr<T> (xdr::xdr_istream&)> decoder_;
  bool enable_compression_;
  mutable std::mutex lck_;
  mutable std::weak_ptr<const T> decode_result_;
};

template<typename T, typename Hasher = std::hash<T>>
class monsoon_dirhistory_local_ dictionary {
 public:
  std::uint32_t encode(const T&);
  const T& decode(std::uint32_t) const;

  template<typename SerFn>
  void decode_update(xdr::xdr_istream&, SerFn);
  template<typename SerFn>
  void encode_update(xdr::xdr_ostream&, SerFn);

  bool update_pending() const noexcept;

 private:
  std::vector<T> decode_map_;
  std::unordered_map<T, std::uint32_t, Hasher> encode_map_;
  std::uint32_t update_start_ = 0;
};

class monsoon_dirhistory_local_ dictionary_delta {
 private:
  struct strvector_hasher {
    template<typename Alloc1, typename Alloc2, typename Char, typename CharT>
    std::size_t operator()(
        const std::vector<std::basic_string<Char, CharT, Alloc2>, Alloc1>&)
    const noexcept;
  };

 public:
  void decode_update(xdr::xdr_istream&);
  void encode_update(xdr::xdr_ostream&);

  dictionary<std::string> sdd;
  dictionary<std::vector<std::string>, strvector_hasher> pdd;
  dictionary<tags> tdd;
};

/** The TSData structure of the 'list' implementation. */
class monsoon_dirhistory_local_ tsdata_list
: public std::enable_shared_from_this<tsdata_list>
{
 public:
  using record_array = std::unordered_map<
      group_name,
      file_segment<time_series_value::metric_map>>;

  tsdata_list() = default;
  tsdata_list(tsdata_list&&) = delete;
  tsdata_list& operator=(tsdata_list&&) = delete;
  tsdata_list(
      const encdec_ctx&,
      time_point ts,
      std::optional<file_segment_ptr> pred,
      std::optional<file_segment_ptr> dd,
      file_segment_ptr records,
      std::uint32_t reserved) noexcept;
  ~tsdata_list() noexcept;

  time_point ts() const { return ts_; }
  std::shared_ptr<tsdata_list> pred() const;
  std::shared_ptr<record_array> records(const dictionary_delta&) const;
  std::shared_ptr<dictionary_delta> dictionary() const;

 private:
  time_point ts_;
  std::optional<file_segment_ptr> pred_;
  std::optional<file_segment_ptr> dd_;
  file_segment_ptr records_;
  std::uint32_t reserved_;
  mutable std::weak_ptr<tsdata_list> cached_pred_;
  mutable std::weak_ptr<record_array> cached_records_;
  mutable std::mutex lock_;
  const encdec_ctx ctx_;
};

using metric_table = std::vector<std::optional<metric_value>>;
using group_table = std::tuple<std::vector<bool>, std::unordered_map<metric_name, file_segment<metric_table>>>;
using tables = std::unordered_map<group_name, file_segment<group_table>>;
using file_data_tables_block = std::tuple<std::vector<time_point>, file_segment<tables>>;
using file_data_tables = std::vector<file_data_tables_block>;

class monsoon_dirhistory_local_ tsfile_header {
 public:
  static constexpr std::size_t XDR_SIZE = 16 + 4 + 4 + 8 + 16;

  using kind_variant =
      std::variant<file_segment<tsdata_list>, file_segment<file_data_tables>>;

  tsfile_header(xdr::xdr_istream&, std::shared_ptr<io::fd>);
  ~tsfile_header() noexcept;

  const kind_variant& fdt() const & {
    return fdt_;
  }

  kind_variant& fdt() & {
    return fdt_;
  }

  kind_variant&& fdt() && {
    return std::move(fdt_);
  }

  const time_point& first() const { return first_; }
  const time_point& last() const { return last_; }
  const std::uint32_t& flags() const { return flags_; }
  const std::uint64_t& file_size() const { return file_size_; }

 private:
  time_point first_, last_; // 16 bytes (8 each)
  std::uint32_t flags_; // 4 bytes
  std::uint32_t reserved_; // 4 bytes
  std::uint64_t file_size_; // 8 bytes
  kind_variant fdt_; // 16 bytes (underlying file pointer)
};


monsoon_dirhistory_local_
time_point decode_timestamp(xdr::xdr_istream&);
monsoon_dirhistory_local_
void encode_timestamp(xdr::xdr_ostream&, time_point);

monsoon_dirhistory_local_
std::vector<time_point> decode_timestamp_delta(xdr::xdr_istream&);
monsoon_dirhistory_local_
void encode_timestamp_delta(xdr::xdr_ostream&, std::vector<time_point>&&);

monsoon_dirhistory_local_
std::vector<bool> decode_bitset(xdr::xdr_istream&);
monsoon_dirhistory_local_
void encode_bitset(xdr::xdr_ostream&, const std::vector<bool>&);

monsoon_dirhistory_local_
histogram decode_histogram(xdr::xdr_istream&);
monsoon_dirhistory_local_
void encode_histogram(xdr::xdr_ostream&, const histogram&);

monsoon_dirhistory_local_
metric_value decode_metric_value(xdr::xdr_istream&,
    const dictionary<std::string>&);
monsoon_dirhistory_local_
void encode_metric_value(xdr::xdr_ostream&,
    const metric_value& value, dictionary<std::string>&);

monsoon_dirhistory_local_
file_segment_ptr decode_file_segment(xdr::xdr_istream&);
monsoon_dirhistory_local_
void encode_file_segment(xdr::xdr_ostream&, const file_segment_ptr&);

monsoon_dirhistory_local_
auto decode_record_metrics(xdr::xdr_istream&, const dictionary_delta&)
  -> std::shared_ptr<time_series_value::metric_map>;
monsoon_dirhistory_local_
void encode_record_metrics(xdr::xdr_ostream&,
    const time_series_value::metric_map&,
    dictionary_delta&);

monsoon_dirhistory_local_
auto decode_record_array(xdr::xdr_istream&, const encdec_ctx&,
    const dictionary_delta&)
  -> tsdata_list::record_array;
monsoon_dirhistory_local_
void encode_record_array(xdr::xdr_ostream&,
    const std::vector<std::pair<group_name, file_segment_ptr>>&,
    dictionary_delta&);

monsoon_dirhistory_local_
auto decode_tsdata(xdr::xdr_istream&, const encdec_ctx&)
  -> std::shared_ptr<tsdata_list>;

monsoon_dirhistory_local_
auto decode_file_data_tables_block(xdr::xdr_istream&, const encdec_ctx&)
  -> file_data_tables_block;

monsoon_dirhistory_local_
auto decode_file_data_tables(xdr::xdr_istream&, const encdec_ctx&)
  -> std::shared_ptr<file_data_tables>;

monsoon_dirhistory_local_
auto decode_tables(xdr::xdr_istream&, const encdec_ctx&,
    const std::shared_ptr<const dictionary_delta>&)
  -> std::shared_ptr<tables>;
monsoon_dirhistory_local_
void encode_tables(xdr::xdr_ostream&,
    const std::unordered_map<group_name, file_segment_ptr>&,
    dictionary_delta&);

monsoon_dirhistory_local_
auto decode_group_table(xdr::xdr_istream&,
    const encdec_ctx&, const std::shared_ptr<const dictionary_delta>&)
  -> std::shared_ptr<group_table>;
monsoon_dirhistory_local_
void encode_group_table(xdr::xdr_ostream&,
    const std::vector<bool>&,
    const std::vector<std::pair<metric_name, file_segment_ptr>>&,
    dictionary_delta&);

monsoon_dirhistory_local_
auto decode_metric_table(xdr::xdr_istream&,
    const std::shared_ptr<const dictionary<std::string>>&)
  -> std::shared_ptr<metric_table>;
monsoon_dirhistory_local_
void write_metric_table(xdr::xdr_ostream&,
    const std::vector<std::optional<metric_value>>&,
    dictionary<std::string>&);


}}} /* namespace monsoon::history::v2 */

#include "encdec-inl.h"

#endif /* V2_ENCDEC_H */