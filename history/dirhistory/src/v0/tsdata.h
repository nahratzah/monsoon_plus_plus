#ifndef V0_TSDATA_H
#define V0_TSDATA_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <cstdint>
#include <string>
#include <vector>
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

namespace monsoon {
namespace history {
namespace v0 {


monsoon_dirhistory_local_
std::vector<std::string> decode_path(monsoon::xdr::xdr_istream&);
monsoon_dirhistory_local_
void encode_path(monsoon::xdr::xdr_ostream&, const std::vector<std::string>&);

enum class metrickind : std::uint32_t {
  BOOL = 0,
  INT = 1,
  FLOAT = 2,
  STRING = 3,
  HISTOGRAM = 4,
  EMPTY = 0x7fffffff
};

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

#endif /* V0_TSDATA_H */
