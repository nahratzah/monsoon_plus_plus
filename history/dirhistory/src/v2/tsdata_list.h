#ifndef V2_TSDATA_LIST_H
#define V2_TSDATA_LIST_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <monsoon/xdr/xdr.h>
#include <monsoon/time_point.h>
#include "encdec.h"
#include "tsdata.h"

namespace monsoon {
namespace history {
namespace v2 {


class monsoon_dirhistory_local_ tsdata_v2_list
: public tsdata_v2,
  public std::enable_shared_from_this<tsdata_v2_list>
{
 public:
  tsdata_v2_list(io::fd&& fd, const tsfile_mimeheader& mime, const tsfile_header& hdr)
  : tsdata_v2(std::move(fd), mime, hdr)
  {}

  ~tsdata_v2_list() noexcept override;

  bool is_writable() const noexcept override;
  void push_back(const time_series&);
  void push_back(const emit_type&) override;

  auto emit(
      std::optional<time_point>,
      std::optional<time_point>,
      const path_matcher&,
      const tag_matcher&,
      const path_matcher&)
      const
  -> objpipe::reader<emit_type> override;
  auto emit_time(
      std::optional<time_point>,
      std::optional<time_point>) const
  -> objpipe::reader<time_point> override;

 private:
  auto read_() const -> std::shared_ptr<tsdata_xdr>;
  std::vector<time_series> read_all_raw_() const override;

  template<typename Callback>
  auto direct_emit_cb_(
      std::optional<time_point> begin,
      std::optional<time_point> end,
      const path_matcher& group_filter,
      const tag_matcher& tag_filter,
      const path_matcher& metric_filter,
      Callback cb) const
  -> void;

  template<typename Callback>
  auto emit_cb_sorted_with_duplicates_(
      std::optional<time_point> begin,
      std::optional<time_point> end,
      const path_matcher& group_filter,
      const tag_matcher& tag_filter,
      const path_matcher& metric_filter,
      Callback cb) const
  -> void;

  template<typename Callback>
  auto emit_cb_unsorted_(
      std::optional<time_point> begin,
      std::optional<time_point> end,
      const path_matcher& group_filter,
      const tag_matcher& tag_filter,
      const path_matcher& metric_filter,
      Callback cb) const
  -> void;

  template<typename Fn>
  void visit(Fn) const;
};


}}} /* namespace monsoon::history::v2 */

#include "tsdata_list-inl.h"

#endif /* V2_TSDATA_LIST_H */
