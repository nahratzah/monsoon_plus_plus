#ifndef V2_FILE_SEGMENT_PTR_H
#define V2_FILE_SEGMENT_PTR_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <monsoon/io/fd.h>
#include <monsoon/xdr/xdr.h>
#include <cstddef>

namespace monsoon {
namespace history {
namespace v2 {


class monsoon_dirhistory_local_ file_segment_ptr {
 public:
  using offset_type = io::fd::offset_type;
  using size_type = io::fd::size_type;

  constexpr file_segment_ptr() noexcept = default;

  constexpr file_segment_ptr(offset_type off, size_type len) noexcept
  : off_(off),
    len_(len)
  {}

  constexpr offset_type offset() const noexcept { return off_; }
  constexpr size_type size() const noexcept { return len_; }

  static auto from_xdr(xdr::xdr_istream& in) -> file_segment_ptr {
    file_segment_ptr result;
    result.decode(in);
    return result;
  }

  auto decode(xdr::xdr_istream& in) -> void {
    off_ = in.get_uint64();
    len_ = in.get_uint64();
  }

  auto encode(xdr::xdr_ostream& out) const -> void {
    out.put_uint64(off_);
    out.put_uint64(len_);
  }

  constexpr auto operator==(const file_segment_ptr& y) const
  noexcept
  -> bool {
    return off_ == y.off_ && len_ == y.len_;
  }

  constexpr auto operator!=(const file_segment_ptr& y) const
  noexcept
  -> bool {
    return !(*this == y);
  }

 private:
  offset_type off_ = 0;
  size_type len_ = 0;
};


}}} /* namespace monsoon::history::v2 */

namespace std {


template<>
struct monsoon_dirhistory_local_ hash<monsoon::history::v2::file_segment_ptr> {
  auto operator()(const monsoon::history::v2::file_segment_ptr& fptr) const
  noexcept
  -> std::size_t {
    std::hash<monsoon::history::v2::file_segment_ptr::offset_type> off_hash;
    std::hash<monsoon::history::v2::file_segment_ptr::size_type> sz_hash;

    return (73u * off_hash(fptr.offset())) ^ sz_hash(fptr.size());
  }
};


} /* namespace std */

#endif /* V2_FILE_SEGMENT_PTR_H */
