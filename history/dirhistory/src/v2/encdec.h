#ifndef V2_ENCDEC_H
#define V2_ENCDEC_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <monsoon/xdr/xdr.h>
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

namespace monsoon {
namespace history {
namespace v2 {
namespace header_flags {


                        /* KIND: indicate if the type of file data. */
constexpr std::uint32_t KIND_MASK        = 0x0000000f,
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


class encdec_ctx {
 public:
  std::shared_ptr<io::fd> fd() const noexcept { return fd_; }

 private:
  std::shared_ptr<io::fd> fd_;
};

class file_segment_ptr {
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
class file_segment {
 public:
  using offset_type = file_segment_ptr::offset_type;
  using size_type = file_segment_ptr::size_type;

  file_segment() = default;
  file_segment(const file_segment&) = delete;
  file_segment(file_segment&&) noexcept;
  file_segment& operator=(const file_segment&) = delete;
  file_segment& operator=(file_segment&&) noexcept;
  file_segment(const encdec_ctx&, file_segment_ptr,
      std::function<T (xdr::xdr_istream&)>&&, bool = true) noexcept;

  T operator*() const;
  T get() const;

 private:
  file_segment_ptr ptr_;
  encdec_ctx ctx_;
  std::function<T (xdr::xdr_istream&)> decoder_;
  bool enable_compression_;
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
class tsdata_list {
 public:
  using record_array = std::unordered_map<
      group_name,
      file_segment<time_series_value::metric_map>>;

  tsdata_list() = default;
  tsdata_list(tsdata_list&&) noexcept;
  tsdata_list& operator=(tsdata_list&&) noexcept;
  tsdata_list(
      time_point ts,
      optional<file_segment<tsdata_list>>&& pred,
      optional<file_segment<dictionary_delta>>&& dd,
      file_segment<record_array>&& records,
      std::uint32_t reserved) noexcept;
  ~tsdata_list() noexcept;

  time_point ts() { return ts_; }
  const optional<file_segment<tsdata_list>>& pred() const { return pred_; }
  const optional<file_segment<dictionary_delta>>& dd() const { return dd_; }
  const file_segment<record_array>& records() const { return records_; }

 private:
  time_point ts_;
  optional<file_segment<tsdata_list>> pred_;
  optional<file_segment<dictionary_delta>> dd_;
  file_segment<record_array> records_;
  std::uint32_t reserved_;
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
time_series_value::metric_map decode_record_metrics(xdr::xdr_istream&,
    const dictionary_delta&);
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


}}} /* namespace monsoon::history::v2 */

#include "encdec-inl.h"

#endif /* V2_ENCDEC_H */
