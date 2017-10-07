#ifndef V2_FILE_SEGMENT_PTR_H
#define V2_FILE_SEGMENT_PTR_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <monsoon/io/fd.h>

namespace monsoon {
namespace history {
namespace v2 {


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


}}} /* namespace monsoon::history::v2 */

#endif /* V2_FILE_SEGMENT_PTR_H */
