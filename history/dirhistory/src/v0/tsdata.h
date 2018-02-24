#ifndef V0_TSDATA_H
#define V0_TSDATA_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <vector>
#include <monsoon/io/fd.h>
#include <monsoon/simple_group.h>
#include <monsoon/group_name.h>
#include <monsoon/metric_name.h>
#include <monsoon/metric_value.h>
#include <monsoon/tags.h>
#include <monsoon/time_point.h>
#include <monsoon/time_series.h>
#include <monsoon/time_series_value.h>
#include <monsoon/histogram.h>
#include <monsoon/xdr/xdr.h>
#include <monsoon/history/dir/tsdata.h>

namespace monsoon {
namespace history {
namespace v0 {


class monsoon_dirhistory_local_ tsdata_v0
: public tsdata,
  public std::enable_shared_from_this<tsdata_v0>
{
 public:
  static constexpr std::uint16_t MAJOR = 0u;
  static constexpr std::uint16_t MAX_MINOR = 1u;

  tsdata_v0(io::fd&& file);
  ~tsdata_v0() noexcept;

  auto read_all() const -> std::vector<time_series> override;
  static std::shared_ptr<tsdata_v0> write_all(const std::string&,
      std::vector<time_series>&&, bool);
  auto version() const noexcept -> std::tuple<std::uint16_t, std::uint16_t>
      override;
  bool is_writable() const noexcept override;
  void push_back(const time_series&) override;
  auto time() const -> std::tuple<time_point, time_point> override;

  static std::shared_ptr<tsdata_v0> new_file(io::fd&&, time_point tp);

  auto emit(
      std::optional<time_point>,
      std::optional<time_point>,
      const path_matcher&,
      const tag_matcher&,
      const path_matcher&) const
  -> objpipe::reader<emit_type> override;
  auto emit_time(
      std::optional<time_point>,
      std::optional<time_point>) const
  -> objpipe::reader<time_point> override;

 private:
  auto make_xdr_istream(bool) const -> std::unique_ptr<xdr::xdr_istream>;

  template<typename Fn>
  void visit(Fn) const;

  io::fd file_;
  const bool gzipped_;
  time_point tp_begin_, tp_end_;
  std::uint16_t minor_version;
};


monsoon_dirhistory_local_
auto decode_tsfile_header(monsoon::xdr::xdr_istream&)
  -> std::tuple<time_point, time_point>;
monsoon_dirhistory_local_
void encode_tsfile_header(monsoon::xdr::xdr_ostream&,
    std::tuple<time_point, time_point>);

monsoon_dirhistory_local_
std::vector<std::string> decode_path(monsoon::xdr::xdr_istream&);
monsoon_dirhistory_local_
void encode_path(monsoon::xdr::xdr_ostream&, const std::vector<std::string>&);

monsoon_dirhistory_local_
metric_value decode_metric_value(monsoon::xdr::xdr_istream&);
monsoon_dirhistory_local_
void encode_metric_value(monsoon::xdr::xdr_ostream&,
    const metric_value&);

monsoon_dirhistory_local_
histogram decode_histogram(monsoon::xdr::xdr_istream&);
monsoon_dirhistory_local_
void encode_histogram(monsoon::xdr::xdr_ostream&,
    const histogram&);

monsoon_dirhistory_local_
time_series decode_time_series(monsoon::xdr::xdr_istream&);
monsoon_dirhistory_local_
void encode_time_series(monsoon::xdr::xdr_ostream&, const time_series&);

using metric_map_entry = std::pair<metric_name, metric_value>;

monsoon_dirhistory_local_
std::vector<metric_map_entry> decode_metric_map(monsoon::xdr::xdr_istream&);

monsoon_dirhistory_local_
time_point decode_timestamp(monsoon::xdr::xdr_istream&);
monsoon_dirhistory_local_
void encode_timestamp(monsoon::xdr::xdr_ostream&, const time_point&);

monsoon_dirhistory_local_
tags decode_tags(monsoon::xdr::xdr_istream&);
monsoon_dirhistory_local_
void encode_tags(monsoon::xdr::xdr_ostream&, const tags&);


}}} /* namespace monsoon::history::v0 */

#include "tsdata-inl.h"

#endif /* V0_TSDATA_H */
